from __future__ import annotations
from .run_crossover_large_config import (
    MPS_LINE_BUDGET, OBJ_ROW, json, math, os, random, subprocess, time
)

def q10(value: float) -> float:
    """Round to 10 significant digits, the precision actually written to file."""
    return float("%.10g" % value)

def fmt(value: float) -> str:
    return "%.10g" % value

def varname(j: int) -> str:
    return "X%06d" % j

def rowname(i: int) -> str:
    return "R%05d" % i

def build_instance(m: int, k: int, seed: int):
    """Return (objective, row_entries, rhs) for the banded feasibility LP.

    row_entries[i][t] is the value of column i+t in row i (t in 0..k).
    """
    rng = random.Random(seed)
    n = m + k
    objective = [q10(rng.uniform(1.0, 2.0)) for _ in range(n)]
    row_entries = []
    rhs = []
    for _i in range(m):
        vals = [0.0] * (k + 1)
        off_sum = 0.0
        for t in range(1, k + 1):
            v = q10(rng.uniform(0.5, 1.5) / k)
            vals[t] = v
            off_sum += v
        vals[0] = q10(1.0 + off_sum)          # strictly dominant diagonal
        row_entries.append(vals)
        rhs.append(q10(vals[0] + off_sum))    # b = A * 1^n
    return objective, row_entries, rhs

def nnz_of(m: int, k: int) -> int:
    return m * (k + 1) + (m + k)

def estimated_mps_lines(m: int, k: int) -> int:
    entries = nnz_of(m, k)
    return math.ceil(entries / 2) + (m + 2) + math.ceil(m / 2) + 6

def write_mps(path: str, name: str, m: int, k: int, objective, row_entries, rhs) -> None:
    n = m + k
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("NAME %s\nROWS\n" % name)
        fh.write(" N %s\n" % OBJ_ROW)
        for i in range(m):
            fh.write(" E %s\n" % rowname(i))
        fh.write("COLUMNS\n")
        for j in range(n):
            pairs = [(OBJ_ROW, objective[j])]
            for i in range(max(0, j - k), min(m, j + 1)):
                pairs.append((rowname(i), row_entries[i][j - i]))
            var = varname(j)
            for s in range(0, len(pairs), 2):
                chunk = pairs[s:s + 2]
                body = " ".join("%s %s" % (r, fmt(v)) for r, v in chunk)
                fh.write(" %s %s\n" % (var, body))
        fh.write("RHS\n")
        for s in range(0, m, 2):
            chunk = [(rowname(i), rhs[i]) for i in range(s, min(s + 2, m))]
            body = " ".join("%s %s" % (r, fmt(v)) for r, v in chunk)
            fh.write(" RHS1 %s\n" % body)
        fh.write("ENDATA\n")

def write_lp(path: str, name: str, m: int, k: int, objective, row_entries, rhs) -> None:
    n = m + k
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("Minimize\n obj:")
        for s in range(0, n, 8):
            terms = ["%s %s" % (fmt(objective[j]), varname(j))
                     for j in range(s, min(s + 8, n))]
            fh.write("\n  " + " + ".join(terms))
        fh.write("\nSubject To\n")
        for i in range(m):
            vals = row_entries[i]
            terms = ["%s %s" % (fmt(vals[t]), varname(i + t)) for t in range(k + 1)]
            fh.write(" %s: " % rowname(i))
            for s in range(0, len(terms), 8):
                fh.write(" + ".join(terms[s:s + 8]))
                if s + 8 < len(terms):
                    fh.write("\n   ")
            fh.write(" = %s\n" % fmt(rhs[i]))
        fh.write("End\n")

def generate_instance(name: str, m: int, k: int, seed: int, out_dir: str) -> dict:
    """Generate (or reuse) the instance file; returns path/nnz metadata."""
    use_mps = estimated_mps_lines(m, k) <= MPS_LINE_BUDGET
    ext = "mps" if use_mps else "lp"
    path = os.path.join(out_dir, "%s.%s" % (name, ext))
    n = m + k
    nnz = nnz_of(m, k)
    # Always regenerate. The same instance names were previously emitted by
    # more than one generator with incompatible matrices, so existence alone
    # is not a safe cache key.
    objective, row_entries, rhs = build_instance(m, k, seed)
    started = time.perf_counter()
    if use_mps:
        write_mps(path, name, m, k, objective, row_entries, rhs)
    else:
        write_lp(path, name, m, k, objective, row_entries, rhs)
    gen_s = time.perf_counter() - started
    print("generated %s: rows=%d cols=%d nnz=%d fmt=%s (%.1fs, %d lines est)" %
          (path, m, n, nnz, ext, gen_s, estimated_mps_lines(m, k)), flush=True)
    return {"name": name, "path": path, "rows": m, "cols": n, "nnz": nnz,
            "format": ext, "seed": seed}

def probe_cuda(solver: str) -> tuple[bool, str]:
    """Decide whether the binary really executes on a GPU.

    A --backend gpu run always "succeeds" on a CUDA-less build (the PDHG path
    falls back to host memory), so a successful run proves nothing; linkage is
    what matters. Probe the sibling CMakeCache.txt first, then ldd.
    """
    solver_dir = os.path.dirname(os.path.abspath(solver))
    cache = os.path.join(solver_dir, "CMakeCache.txt")
    if os.path.exists(cache):
        try:
            with open(cache, "r", encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            text = ""
        if "MARKOV_CERO_ENABLE_CUDA:BOOL=OFF" in text:
            return False, ("MARKOV_CERO_ENABLE_CUDA=BOOL=OFF in %s and no CUDA "
                           "language enabled; --backend gpu would only run the "
                           "host-emulated PDHG path" % cache)
        if "MARKOV_CERO_ENABLE_CUDA:BOOL=ON" in text:
            return True, "MARKOV_CERO_ENABLE_CUDA=ON in %s" % cache
    try:
        out = subprocess.run(["ldd", solver], capture_output=True, text=True,
                             timeout=30)
    except (OSError, subprocess.SubprocessError):
        return False, "unable to inspect linkage (ldd failed)"
    blob = (out.stdout or "") + (out.stderr or "")
    if "libcudart" in blob or "libcuda.so" in blob:
        return True, "ldd shows CUDA runtime linked"
    return False, "ldd shows no CUDA runtime linked into %s" % solver

def parse_json_line(stdout: str) -> dict:
    for line in stdout.splitlines():
        stripped = line.strip()
        if stripped.startswith("{"):
            try:
                payload = json.loads(stripped)
            except json.JSONDecodeError:
                continue
            if isinstance(payload, dict):
                return payload
    return {}

def solve_once(solver: str, instance_path: str, engine: str, backend: str,
               timeout: float, log_dir: str | None, tag: str) -> dict:
    cmd = [solver, instance_path, "--engine", engine, "--backend", backend,
           "--time-limit", "%g" % timeout]
    started = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        if log_dir:
            with open(os.path.join(log_dir, tag + ".log"), "w",
                      encoding="utf-8") as fh:
                fh.write("cmd: %s\nstatus: killed at %.1fs wall cap "
                         "(LP engines do not enforce --time-limit)\n"
                         % (" ".join(cmd), timeout))
        return {"status": "TimeLimit", "verified": False, "total_ms": elapsed_ms}
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    payload = parse_json_line(proc.stdout)
    if log_dir:
        with open(os.path.join(log_dir, tag + ".log"), "w",
                  encoding="utf-8") as fh:
            fh.write("cmd: %s\nreturncode: %d\nstdout(first json): %s\nstderr:\n%s\n"
                     % (" ".join(cmd), proc.returncode,
                        json.dumps(payload) if payload else "<none>",
                        (proc.stderr or "")[-4000:]))
    if payload:
        status = str(payload.get("status", "Unknown"))
        verified = bool(payload.get("verified", False))
    elif proc.returncode != 0:
        status = "Error"
        verified = False
    else:
        status = "Unknown"
        verified = False
    return {"status": status, "verified": verified, "total_ms": elapsed_ms}
