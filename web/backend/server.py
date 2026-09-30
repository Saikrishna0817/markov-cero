"""Bounded HTTP adapter for the existing markov-cero CLI solver."""

import json
import os
import pathlib
import subprocess
import sys
import tempfile
import threading
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


PORT = int(os.environ.get("PORT", "8080"))
SOLVER_BIN = os.environ.get("SOLVER_BIN", "/app/markov-cero-solve")
SUPABASE_URL = os.environ.get("SUPABASE_URL", "").rstrip("/")
SUPABASE_KEY = os.environ.get("SUPABASE_PUBLISHABLE_KEY", "")
ALLOWED_ORIGIN = os.environ.get("ALLOWED_ORIGIN", "")
MAX_BODY = 1_100_000
MAX_MODEL = 1_000_000
MAX_RESULT = 2_000_000
SLOTS = threading.BoundedSemaphore(2)

# OS hard limits for every spawned solve (RES-01 step 5,
# docs/contracts/resource-limits.md): the CLI flags below are cooperative,
# these bounds are enforced by the kernel and survive a solver that never
# polls. Wall kills the child; CPU, address-space and file-size limits are
# applied by exec'ing the solver from a thin rlimit wrapper so a threaded
# server never forks with preexec hooks. Output is bounded by redirecting
# stdout/stderr into files that RLIMIT_FSIZE caps.
SOLVE_WALL_TIMEOUT_SECONDS = float(os.environ.get("SOLVE_WALL_TIMEOUT_SECONDS", "15"))
SOLVE_RLIMIT_CPU_SECONDS = int(os.environ.get("SOLVE_RLIMIT_CPU_SECONDS", "12"))
SOLVE_RLIMIT_AS_BYTES = int(os.environ.get("SOLVE_RLIMIT_AS_BYTES", str(1024 ** 3)))
SOLVE_RLIMIT_FSIZE_BYTES = int(os.environ.get("SOLVE_RLIMIT_FSIZE_BYTES", str(16 * 1024 ** 2)))
MAX_DETAIL_BYTES = 4096

# argv: program cpu_seconds as_bytes fsize_bytes [program args...]
_RLIMIT_WRAPPER = (
    "import os,resource,sys;"
    "_c,_a,_f=int(sys.argv[2]),int(sys.argv[3]),int(sys.argv[4]);"
    "resource.setrlimit(resource.RLIMIT_CPU,(_c,_c+3));"
    "resource.setrlimit(resource.RLIMIT_AS,(_a,_a));"
    "resource.setrlimit(resource.RLIMIT_FSIZE,(_f,_f));"
    "os.execvp(sys.argv[1],[sys.argv[1]]+sys.argv[5:])"
)


def run_solver(command, timeout, cpu_seconds, as_bytes, fsize_bytes,
               stdout_path, stderr_path):
    """Run one solve under the declared OS hard limits.

    Raises subprocess.TimeoutExpired (after killing the child) on the wall
    limit. A negative returncode means the kernel terminated the child
    (CPU/address-space/file-size signal). stdout/stderr land in the given
    files, bounded by fsize_bytes, and are read back by the caller.
    """
    wrapped = [sys.executable, "-c", _RLIMIT_WRAPPER, command[0],
               str(cpu_seconds), str(as_bytes), str(fsize_bytes), *command[1:]]
    with open(stdout_path, "wb") as out, open(stderr_path, "wb") as err:
        return subprocess.run(wrapped, stdout=out, stderr=err, timeout=timeout,
                              check=False)


def read_capped(path):
    try:
        return pathlib.Path(path).read_bytes()[:MAX_DETAIL_BYTES].decode(
            "utf-8", errors="replace")
    except OSError:
        return ""


class Handler(BaseHTTPRequestHandler):
    server_version = "MarkovCeroAPI/0.1"

    def end_headers(self):
        origin = self.headers.get("Origin", "")
        if ALLOWED_ORIGIN and origin == ALLOWED_ORIGIN:
            self.send_header("Access-Control-Allow-Origin", ALLOWED_ORIGIN)
            self.send_header("Access-Control-Allow-Headers", "authorization, content-type")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Vary", "Origin")
        self.send_header("X-Content-Type-Options", "nosniff")
        super().end_headers()

    def respond(self, status, data):
        payload = json.dumps(data, separators=(",", ":"), allow_nan=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_OPTIONS(self):
        if self.path != "/solve" or self.headers.get("Origin", "") != ALLOWED_ORIGIN:
            return self.respond(403, {"error": "Origin is not allowed."})
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        if self.path != "/health":
            return self.respond(404, {"error": "Not found."})
        ready = pathlib.Path(SOLVER_BIN).is_file() and os.access(SOLVER_BIN, os.X_OK)
        self.respond(200 if ready else 503, {"status": "ready" if ready else "solver_unavailable"})

    def authenticated(self):
        token_header = self.headers.get("Authorization", "")
        if not token_header.startswith("Bearer ") or not SUPABASE_URL or not SUPABASE_KEY:
            return False
        token = token_header[7:].strip()
        if not token or len(token) > 4096:
            return False
        request = urllib.request.Request(
            f"{SUPABASE_URL}/auth/v1/user",
            headers={"apikey": SUPABASE_KEY, "Authorization": f"Bearer {token}"},
        )
        try:
            with urllib.request.urlopen(request, timeout=5) as response:
                user = json.load(response)
                return bool(user.get("id"))
        except (urllib.error.URLError, ValueError, TimeoutError):
            return False

    def do_POST(self):
        if self.path != "/solve":
            return self.respond(404, {"error": "Not found."})
        if ALLOWED_ORIGIN and self.headers.get("Origin") not in (None, ALLOWED_ORIGIN):
            return self.respond(403, {"error": "Origin is not allowed."})
        if not self.authenticated():
            return self.respond(401, {"error": "A valid session is required."})
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            return self.respond(400, {"error": "Invalid content length."})
        if length < 1 or length > MAX_BODY:
            return self.respond(413, {"error": "Model request exceeds the size limit."})
        if not self.headers.get("Content-Type", "").startswith("application/json"):
            return self.respond(415, {"error": "Send JSON."})
        try:
            body = json.loads(self.rfile.read(length))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return self.respond(400, {"error": "Invalid JSON."})
        model = body.get("model") if isinstance(body, dict) else None
        filename = body.get("filename", "model.mps") if isinstance(body, dict) else ""
        if not isinstance(model, str) or not model.strip() or len(model.encode("utf-8")) > MAX_MODEL:
            return self.respond(400, {"error": "Model must be nonempty text under 1 MB."})
        suffix = pathlib.Path(filename).suffix.lower() if isinstance(filename, str) else ""
        if suffix not in (".mps", ".lp"):
            return self.respond(400, {"error": "Use an .mps or .lp model."})
        if not SLOTS.acquire(blocking=False):
            return self.respond(429, {"error": "Solver is busy. Try again shortly."})
        try:
            self.solve(model, suffix)
        finally:
            SLOTS.release()

    def solve(self, model, suffix):
        if not pathlib.Path(SOLVER_BIN).is_file():
            return self.respond(503, {"error": "Solver binary is unavailable."})
        with tempfile.TemporaryDirectory(prefix="markov-solve-") as directory:
            source = pathlib.Path(directory) / f"model{suffix}"
            result = pathlib.Path(directory) / "result.json"
            source.write_text(model, encoding="utf-8")
            command = [SOLVER_BIN, str(source), "--output", str(result), "--threads", "1", "--max-nodes", "1000", "--max-queued-nodes", "1000", "--max-input-bytes", "1000000", "--time-limit", "10"]
            try:
                run = run_solver(
                    command,
                    timeout=SOLVE_WALL_TIMEOUT_SECONDS,
                    cpu_seconds=SOLVE_RLIMIT_CPU_SECONDS,
                    as_bytes=SOLVE_RLIMIT_AS_BYTES,
                    fsize_bytes=SOLVE_RLIMIT_FSIZE_BYTES,
                    stdout_path=pathlib.Path(directory) / "stdout.log",
                    stderr_path=pathlib.Path(directory) / "stderr.log",
                )
            except subprocess.TimeoutExpired:
                return self.respond(504, {"error": "Solve exceeded the service time limit."})
            except OSError:
                return self.respond(503, {"error": "Could not start the solver."})
            if run.returncode < 0:
                return self.respond(422, {
                    "error": "Solver exceeded an OS resource limit.",
                    "detail": f"terminated by signal {-run.returncode}",
                })
            if not result.is_file():
                return self.respond(422, {"error": "Solver did not return a result.", "detail": read_capped(pathlib.Path(directory) / "stderr.log")[-500:]})
            if result.stat().st_size > MAX_RESULT:
                return self.respond(413, {"error": "Result exceeds the response size limit."})
            try:
                data = json.loads(result.read_text(encoding="utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                return self.respond(502, {"error": "Solver returned invalid JSON."})
            fields = ("status", "engine", "problem_class", "objective", "verified", "message", "rows", "cols", "nonzeros", "iterations", "nodes_explored", "optimality_gap")
            self.respond(200, {key: data[key] for key in fields if key in data})


if __name__ == "__main__":
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
