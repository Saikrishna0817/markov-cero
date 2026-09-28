#!/usr/bin/env python3
"""Gap 8 / PROJECT.md feature 13: Empirical Crossover Benchmark (5M-nnz scale-up).

Generates deterministic, well-conditioned synthetic LP instances spanning
~100k / 500k / 1M / 2M / 5M nonzeros, then solves each instance with every
engine/backend the existing solver binary actually supports, streaming one row
per run into evidence/benchmarks/crossover_study.csv with the unchanged schema:

    instance,backend,nnz,status,verified,total_ms

backend tokens written by this script:
    cpu_simplex : --engine primal   (reference revised simplex, CPU)
    cpu_pdlp    : --engine pdlp --backend cpu
    gpu_pdlp    : --engine pdlp --backend gpu  (only when the binary is
                  really linked against CUDA; see probe_cuda() below)

--------------------------------------------------------------------------
Instance generation math (all randomness from random.Random(seed), so the
files are reproducible bit-for-bit):

  * Shape: m equality rows, n = m + k columns, upper-banded A. Row i has
    entries only in columns [i, i+k] (k+1 nonzeros per row), so the leading
    m x m block of A is upper triangular with a nonzero diagonal and A has
    full row rank m (the system is never rank-deficient).
  * Conditioning: off-diagonals a_ij ~ U[0.5, 1.5] / k (each row's off-sum
    stays O(1) no matter how wide the band), diagonal a_ii = 1 + off-sum ->
    A is strictly row diagonally dominant, hence well conditioned (and the
    solver's default Ruiz scaling helps further).
  * Known solution: x* = 1^n and b = A x* (b_i = a_ii + off-sum_i > 0), so
    the feasibility point is known a priori. The objective is
    min c'x with c_j ~ U[1, 2] > 0: with x >= 0 and c strictly positive the
    LP is bounded below and attains its optimum; Ax = b is feasible by
    construction. (b and A are written with 10 significant decimal digits, so
    x* = 1 is feasible to ~1e-9, inside the solver's default tolerance.)
  * nnz accounting follows the historical crossover_study.csv convention:
    every COLUMNS entry counts, objective coefficients included,
    nnz = m*(k+1) + n.

File-format selection: the CLI parser allows 256 MB per file but still only
1,000,000 lines, and MPS packs at most two (row,value) pairs per COLUMNS
record -> MPS is used while the estimated line count stays under ~950k
(about 1.9M nnz) and the CPLEX-LP ".lp" format (no line/byte cap in this
repo's parser) is used above that.

Solver envelopes this script respects (see src/transform/sparse_canonicalize.cpp
and src/io/mps.cpp):
  * the simplex/IPM dense path rejects models with rows > 4096 or
    cols > 16384 (ResourceLimit), so shapes stay inside that envelope and the
    simplex arm of the study is a real attempt, not an instant rejection;
  * LP engines do not enforce --time-limit, so the subprocess timeout is the
    effective per-run cap; a killed run is recorded honestly as
    status=TimeLimit / verified=False.

Usage:
    python3 scripts/run_crossover_large.py \
        --solver ./build_gap/markov-cero-solve \
        --timeout 300 \
        --output evidence/benchmarks/crossover_study.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import random
import subprocess
import sys
import time

FIELDNAMES = ["instance", "backend", "nnz", "status", "verified", "total_ms"]

# (instance name, rows m, band width k, seed) -- tuned so nnz = m*(k+1) + n
# lands on the requested scale (n = m + k columns).
SPECS = [
    ("scale_100k", 500, 198, 101),    # nnz = 100,198
    ("scale_500k", 800, 622, 202),    # nnz = 499,822
    ("scale_1m", 1000, 997, 303),     # nnz = 999,997
    ("scale_2m", 1400, 1426, 404),    # nnz = 2,000,626
    ("scale_5m", 2000, 2497, 505),    # nnz = 5,000,497
]

MPS_LINE_BUDGET = 950_000  # CLI parser leaves maximum_lines at 1,000,000
DEFAULT_TARGET = "/tmp/opencode/crossover_instances"
DEFAULT_LOGS = "/tmp/opencode/crossover_logs"
OBJ_ROW = "COST"


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


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Empirical crossover benchmark: large synthetic LPs "
                    "up to 5M nonzeros")
    parser.add_argument("--solver", default="./build_gap/markov-cero-solve")
    parser.add_argument("--target", default=DEFAULT_TARGET,
                        help="directory for generated instances (keep it under "
                             "/tmp; data/ is off-limits for this study)")
    parser.add_argument("--logs", default=DEFAULT_LOGS)
    parser.add_argument("--timeout", type=float, default=300.0,
                        help="per-run wall-clock cap in seconds")
    parser.add_argument("--output", default="evidence/benchmarks/crossover_study.csv")
    parser.add_argument("--instances", nargs="*", default=[s[0] for s in SPECS])
    parser.add_argument("--append", action="store_true",
                        help="append to an existing CSV instead of replacing it")
    parser.add_argument("--include-gpu", action="store_true",
                        help="run the gpu backend even if the binary looks "
                             "CPU-only (diagnostics only)")
    args = parser.parse_args()

    if not os.path.exists(args.solver):
        print("solver not found: %s" % args.solver, file=sys.stderr)
        return 2
    os.makedirs(args.target, exist_ok=True)
    os.makedirs(args.logs, exist_ok=True)

    runs = [("cpu_pdlp", "pdlp", "cpu"), ("cpu_simplex", "primal", "cpu")]
    gpu_ok, gpu_reason = probe_cuda(args.solver)
    if args.include_gpu:
        gpu_ok, gpu_reason = True, "forced by --include-gpu"
    if gpu_ok:
        runs.append(("gpu_pdlp", "pdlp", "gpu"))
        print("gpu backend: AVAILABLE (%s)" % gpu_reason, flush=True)
    else:
        print("gpu backend: SKIPPED -- %s" % gpu_reason, flush=True)
    print("per-run wall cap: %.0fs; runs per instance: %s"
          % (args.timeout, ", ".join(r[0] for r in runs)), flush=True)

    spec_by_name = {s[0]: s for s in SPECS}
    write_header = True
    mode = "w"
    if args.append and os.path.exists(args.output):
        mode = "a"
        write_header = os.path.getsize(args.output) == 0

    study_started = time.perf_counter()
    counts: dict[str, int] = {}
    achieved = []
    with open(args.output, mode, newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=FIELDNAMES)
        if write_header:
            writer.writeheader()
        for name in args.instances:
            spec = spec_by_name.get(name)
            if spec is None:
                print("unknown instance %r; skipping" % name, file=sys.stderr)
                continue
            _, m, k, seed = spec
            info = generate_instance(name, m, k, seed, args.target)
            achieved.append((name, info["nnz"]))
            for label, engine, backend in runs:
                tag = "%s_%s" % (name, label)
                result = solve_once(args.solver, info["path"], engine, backend,
                                    args.timeout, args.logs, tag)
                row = {"instance": name, "backend": label, "nnz": info["nnz"],
                       "status": result["status"],
                       "verified": str(bool(result["verified"])),
                       "total_ms": repr(result["total_ms"])}
                writer.writerow(row)
                fh.flush()
                counts[label] = counts.get(label, 0) + 1
                print("%-12s %-11s nnz=%-8d %-14s verified=%-5s %10.1f ms"
                      % (name, label, info["nnz"], result["status"],
                         bool(result["verified"]), result["total_ms"]),
                      flush=True)

    wall = time.perf_counter() - study_started
    print("\nstudy wall time: %.1f min" % (wall / 60.0))
    print("rows written to %s: %s" % (args.output,
                                       ", ".join("%s=%d" % kv
                                                 for kv in sorted(counts.items()))))
    print("scales achieved: " +
          ", ".join("%s (%d nnz)" % (n, v) for n, v in achieved))
    if not gpu_ok:
        print("gpu rows skipped: %s" % gpu_reason)
    return 0


if __name__ == "__main__":
    sys.exit(main())
