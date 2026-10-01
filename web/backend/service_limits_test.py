#!/usr/bin/env python3
"""REL-01 (blueprint REL-01 step 6): hosted quota, metrics and result fields.

hosted-limits.md v1.1 section 4 declares a per-client solve window, a bounded
`GET /metrics` document and an additive result-field whitelist. This test
drives the real HTTP adapter over a loopback socket: it proves each declared
limit binds, that the counters report what happened, and that `assurance`
reaches an HTTP consumer exactly as it reaches CLI and Python consumers.

Usage: python3 web/backend/service_limits_test.py /path/to/markov-cero-solve
"""

import json
import pathlib
import resource
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import sanitizer_build  # noqa: E402
import server  # noqa: E402
import service_limits  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]


def require(condition, message):
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def request(url, data=None, headers=None, content_type="application/json"):
    head = {"Content-Type": content_type} if data is not None else {}
    head.update(headers or {})
    req = urllib.request.Request(url, data=data, headers=head)
    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            return response.status, dict(response.headers), response.read()
    except urllib.error.HTTPError as error:
        return error.code, dict(error.headers), error.read()


def solve_payload(path):
    return json.dumps({"model": pathlib.Path(path).read_text(encoding="utf-8"),
                       "filename": pathlib.Path(path).name}).encode("utf-8")


def clear_counters():
    with service_limits._METRICS_LOCK:
        for key in list(service_limits._METRICS):
            service_limits._METRICS[key] = 0
        service_limits._SOLVE_STATUS.clear()
        service_limits.SLOTS_USED = 0


def sanitizer_blocked_by_rlimit_as(solver, model):
    """True when this build dies under the declared RLIMIT_AS and is a
    sanitizer build (hosted-limits section 3).

    The probe child establishes causation - the same binary that dies under
    the declared caps must be the one we retry - while build identity comes
    from sanitizer_build.is_sanitizer_build, because a signal-killed child
    reaches the client as "terminated by signal 11" with no banner on stderr
    to key on.
    """

    def under_service_limits():
        resource.setrlimit(resource.RLIMIT_AS,
                           (server.SOLVE_RLIMIT_AS_BYTES, server.SOLVE_RLIMIT_AS_BYTES))
        resource.setrlimit(resource.RLIMIT_CPU,
                           (server.SOLVE_RLIMIT_CPU_SECONDS, server.SOLVE_RLIMIT_CPU_SECONDS))

    try:
        probe = subprocess.run([solver, str(model)], capture_output=True,
                               timeout=60, preexec_fn=under_service_limits)
    except (OSError, subprocess.TimeoutExpired):
        return False
    return probe.returncode != 0 and sanitizer_build.is_sanitizer_build(solver)


def main():
    solver = sys.argv[1] if len(sys.argv) > 1 else server.SOLVER_BIN
    require(pathlib.Path(solver).is_file(), f"solver binary missing: {solver}")
    server.SOLVER_BIN = solver
    model = ROOT / "examples" / "blend.mps"
    require(model.is_file(), f"control model missing: {model}")

    original_origin = server.ALLOWED_ORIGIN
    original_authenticated = server.Handler.authenticated
    allowed = lambda self: True  # noqa: E731 - loopback test double
    server.Handler.authenticated = allowed
    clear_counters()
    server.QUOTA = service_limits.Quota(1000, 60.0)

    httpd = ThreadingHTTPServer(("127.0.0.1", 0), server.Handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{httpd.server_address[1]}"
    try:
        code, _, _ = request(f"{base}/health")
        require(code == 200, f"/health must answer 200, got {code}")

        # Result fields: assurance and the v1.1 additions reach the client.
        code, _, payload = request(f"{base}/solve", data=solve_payload(model))
        if code != 200 and sanitizer_blocked_by_rlimit_as(solver, model):
            # A sanitizer build reserves ~14 TB of shadow address space and
            # dies under the declared RLIMIT_AS; lift only that bound (§3).
            server.SOLVE_RLIMIT_AS_BYTES = resource.RLIM_INFINITY
            print("note: sanitizer build blocked by the declared RLIMIT_AS; "
                  "retrying with only that bound lifted")
            code, _, payload = request(f"{base}/solve", data=solve_payload(model))
        require(code == 200, f"a real solve must answer 200, got {code}: {payload[:200]}")
        data = json.loads(payload)
        require(data.get("status") == "Optimal",
                f"the control solve must stay Optimal, got {data.get('status')}")
        require(data.get("assurance") in {"optimality_witness_checked",
                                          "canonical_lp_witness", "tree_replayed",
                                          "original_primal_checked", "unverified"},
                f"assurance must reach the HTTP consumer, got {data.get('assurance')!r}")
        require("lp_iterations" in data or "iterations" in data,
                "iteration counters must be forwarded")
        require("relative_gap" in data or "optimality_gap" in data,
                "gap fields must be forwarded")

        # Metrics: readable without authentication and consistent with the run.
        code, _, payload = request(f"{base}/metrics")
        require(code == 200, f"/metrics must answer 200, got {code}")
        metrics = json.loads(payload)
        # The /solve response is written before the handler releases its slot,
        # so a metrics read landing between those two events would still see a
        # busy pool; wait for the release instead of sampling the race once.
        release_deadline = time.time() + 5.0
        while metrics["slots_in_use"] != 0 and time.time() < release_deadline:
            time.sleep(0.05)
            code, _, payload = request(f"{base}/metrics")
            require(code == 200, f"/metrics must answer 200, got {code}")
            metrics = json.loads(payload)
        for name in ("requests_total", "solves_total", "solves_accepted",
                     "solves_by_status", "quota_rejected", "busy_rejected",
                     "auth_rejected", "client_error", "server_error",
                     "os_limit_kills", "solver_failures", "timeouts",
                     "solver_ready", "slots_in_use", "slots_total"):
            require(name in metrics, f"/metrics must expose {name}")
        require(metrics["solves_accepted"] >= 1, "accepted solve must be counted")
        require(metrics["solves_by_status"].get("Optimal", 0) >= 1,
                "solver statuses must be counted per status")
        require(metrics["slots_in_use"] == 0, "slots must be released after a solve")
        require(metrics["slots_total"] == 2, "the declared slot count is two")
        require("model" not in json.dumps(metrics).lower(),
                "metrics must not echo model text")

        # Authentication rejection is counted separately from other 4xx.
        server.Handler.authenticated = lambda self: False  # noqa: E731
        code, _, _ = request(f"{base}/solve", data=solve_payload(model))
        require(code == 401, f"an unauthenticated solve must answer 401, got {code}")

        # Request validation, after the auth gate: content type and JSON.
        server.Handler.authenticated = allowed
        code, _, _ = request(f"{base}/solve", data=b"not json",
                             content_type="text/plain")
        require(code == 415, f"a non-JSON content type must answer 415, got {code}")
        code, _, _ = request(f"{base}/solve", data=b"{not json")
        require(code == 400, f"invalid JSON must answer 400, got {code}")

        # Busy: with no free slot the adapter answers 429 from the semaphore.
        saved, server.SLOTS = server.SLOTS, threading.BoundedSemaphore(0)
        code, _, _ = request(f"{base}/solve", data=solve_payload(model))
        server.SLOTS = saved
        require(code == 429, f"an exhausted slot pool must answer 429, got {code}")

        # Quota: a fixed window refuses the request past the budget, answers
        # Retry-After, and opens again once the window has lapsed.
        server.QUOTA = service_limits.Quota(2, 1.0)
        for _ in range(2):
            code, _, _ = request(f"{base}/solve", data=solve_payload(model))
            require(code == 200, f"requests inside the quota must pass, got {code}")
        code, headers, _ = request(f"{base}/solve", data=solve_payload(model))
        require(code == 429, f"the quota must refuse the third request, got {code}")
        require(int(headers.get("Retry-After", "0")) >= 1,
                "a quota refusal must carry Retry-After")
        time.sleep(1.1)
        code, _, _ = request(f"{base}/solve", data=solve_payload(model))
        require(code == 200, f"the window must reopen after it lapses, got {code}")

        # Origin: when configured, a foreign origin is refused before auth.
        server.ALLOWED_ORIGIN = "https://expected.example"
        code, _, _ = request(f"{base}/solve", data=solve_payload(model),
                             headers={"Origin": "https://evil.example"})
        require(code == 403, f"a foreign origin must answer 403, got {code}")
        server.ALLOWED_ORIGIN = original_origin

        # Unknown path stays a plain 404 and is not counted as a solve.
        code, _, _ = request(f"{base}/nope")
        require(code == 404, f"an unknown path must answer 404, got {code}")

        _, _, payload = request(f"{base}/metrics")
        metrics = json.loads(payload)
        require(metrics["auth_rejected"] >= 1, "auth rejections must be counted")
        require(metrics["quota_rejected"] >= 1, "quota rejections must be counted")
        require(metrics["busy_rejected"] >= 1, "slot rejections must be counted")
        require(metrics["client_error"] >= 3, "4xx responses must be counted")
        # The snapshot is taken before its own response is recorded, so the
        # document does not count itself (hosted-limits v1.1 section 4).
        require(metrics["requests_total"] >= 13, "every other response must be counted")
        require(metrics["solves_total"] > metrics["solves_accepted"],
                "refused solves must still count as received")
    finally:
        httpd.shutdown()
        httpd.server_close()
        server.ALLOWED_ORIGIN = original_origin
        server.Handler.authenticated = original_authenticated

    print("hosted service limits: ok")


if __name__ == "__main__":
    main()
