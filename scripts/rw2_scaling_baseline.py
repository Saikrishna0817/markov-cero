#!/usr/bin/env python3
"""RW-2 scaling measurement harness (throwaway).

Runs markov-cero-solve --engine parallel with 1/2/4 threads over a set of MILP
instances and reports the speedup curve S_p = T1/Tp and efficiency E_p = S_p/p,
mirroring the methodology of evidence/benchmarks/phase4.json option_c so the
before/after numbers are directly comparable.
"""

import argparse
import json
import os
import statistics
import subprocess
import sys
import time

REPEATS = 5


def find_solver():
    for cand in (
        "build-rw2/markov-cero-solve",
        "build/markov-cero-solve",
        "markov-cero-solve",
    ):
        if os.path.isfile(cand) and os.access(cand, os.X_OK):
            return cand
    print("error: solver binary not found", file=sys.stderr)
    sys.exit(1)


def find_instance(name):
    for d in ("data/miplib", "data/netlib", "examples"):
        p = os.path.join(d, name)
        if os.path.isfile(p):
            return p
    print(f"error: instance {name} not found", file=sys.stderr)
    sys.exit(1)


def run_once(solver, mps, threads, timeout):
    cmd = [solver, mps, "--engine", "parallel", "--threads", str(threads)]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        wall_ms = (time.perf_counter() - t0) * 1000.0
    except subprocess.TimeoutExpired:
        return None, None, "timeout"
    status, objective = "Unknown", float("nan")
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            try:
                parsed = json.loads(line)
                status = parsed.get("status", "Unknown")
                objective = parsed.get("objective", float("nan"))
                break
            except Exception:
                pass
    return wall_ms, objective, status


def measure(solver, mps, threads, timeout):
    times, statuses, objectives = [], set(), []
    for _ in range(REPEATS):
        ms, obj, status = run_once(solver, mps, threads, timeout)
        if ms is None:
            return None, "timeout", None
        times.append(ms)
        statuses.add(status)
        objectives.append(obj)
    return statistics.median(times), "/".join(sorted(statuses)), objectives


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--instances", nargs="+", default=["stein9.mps", "flugpl.mps"])
    ap.add_argument("--threads", nargs="+", type=int, default=[1, 2, 4])
    ap.add_argument("--timeout", type=int, default=120)
    ap.add_argument("--json-out", default="")
    args = ap.parse_args()

    solver = find_solver()
    results = {}
    print(f"{'instance':<14} {'p':>2} {'T_ms':>12} {'status':<10} {'speedup':>8} {'eff':>7}")
    print("-" * 52)
    for inst in args.instances:
        mps = find_instance(inst)
        t1, st1, _ = measure(solver, mps, 1, args.timeout)
        if t1 is None:
            print(f"{inst:<14} -- T1 timed out, skipping")
            continue
        row = {}
        for p in args.threads:
            tp, st, objs = (t1, st1, None) if p == 1 else measure(solver, mps, p, args.timeout)
            if tp is None:
                print(f"{inst:<14} {p:>2} {'timeout':>12}")
                continue
            speedup = t1 / tp
            eff = speedup / p
            print(f"{inst:<14} {p:>2} {tp:>12.2f} {st:<10} {speedup:>7.2f}x {eff:>6.1%}")
            row[p] = {"t_ms": round(tp, 3), "speedup": round(speedup, 3),
                      "efficiency": round(eff, 4), "status": st}
        results[inst] = row

    if args.json_out:
        with open(args.json_out, "w") as f:
            json.dump(results, f, indent=2)
            f.write("\n")
        print(f"\n[+] wrote {args.json_out}")


if __name__ == "__main__":
    main()
