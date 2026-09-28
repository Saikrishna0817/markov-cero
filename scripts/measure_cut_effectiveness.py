#!/usr/bin/env python3
"""
markov-cero RW-1 cut-effectiveness measurement (R5 / experiment E6).

Runs each MIPLIB instance twice -- once with --no-cuts (baseline) and once with
the full cut pipeline (root + in-tree separation) -- and records node-count
reduction to evidence/cut_effectiveness.csv.

Exit code 0 iff every run is Optimal + verified + matches the reference optimum,
and at least --min-passing instances reach --min-reduction percent node reduction.
"""

import argparse
import csv
import json
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_miplib import MIPLIB_BENCHMARKS, ensure_instance  # noqa: E402


def run_solver(solver_bin, mps_path, extra_args, timeout_sec=120):
    cmd = [solver_bin, mps_path, "--engine", "milp", "--time-limit", "60"] + extra_args
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_sec)
    except subprocess.TimeoutExpired:
        return {"status": "Timeout", "verified": False, "exit_code": -1, "wall_ms": timeout_sec * 1000.0}
    wall_ms = (time.perf_counter() - t0) * 1000.0

    parsed = None
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            try:
                parsed = json.loads(line)
                break
            except Exception:
                pass
    if parsed is None:
        return {
            "status": "ParseError",
            "verified": False,
            "exit_code": proc.returncode,
            "wall_ms": wall_ms,
            "error": (proc.stderr or proc.stdout)[:200],
        }
    parsed["exit_code"] = proc.returncode
    parsed["wall_ms"] = wall_ms
    return parsed


def main():
    parser = argparse.ArgumentParser(description="markov-cero RW-1 cut effectiveness (R5, E6)")
    parser.add_argument("--solver", default=None, help="Path to markov-cero-solve")
    parser.add_argument("--data-dir", default="data/miplib")
    parser.add_argument("--output", default="evidence/cut_effectiveness.csv")
    parser.add_argument("--instances", nargs="+", default=["stein9", "stein15", "flugpl"])
    parser.add_argument("--min-reduction", type=float, default=20.0,
                        help="Required node reduction percent on a passing instance")
    parser.add_argument("--min-passing", type=int, default=1,
                        help="Number of instances that must reach --min-reduction (ctest gate: 1; "
                             "audit DoD: 2)")
    args = parser.parse_args()
    if not os.path.isabs(args.output):
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        args.output = os.path.join(repo_root, args.output)

    solver = args.solver
    if not solver:
        for c in ["build/markov-cero-solve", "bin/markov-cero-solve", "./markov-cero-solve"]:
            if os.path.isfile(c) and os.access(c, os.X_OK):
                solver = os.path.abspath(c)
                break
    if not solver:
        print("[-] markov-cero-solve executable not found; build first.", file=sys.stderr)
        sys.exit(1)

    print("=== markov-cero RW-1 cut effectiveness (R5 / E6) ===")
    print(f"Solver:  {solver}")
    print(f"Output:  {args.output}")
    print(f"Gate:    >= {args.min_reduction:.1f}% node reduction on >= {args.min_passing} instance(s)")
    print("=" * 100)

    rows = []
    all_ok = True
    passing = 0

    for name in args.instances:
        if name not in MIPLIB_BENCHMARKS:
            print(f"[!] unknown instance {name}, skipping")
            continue
        meta = MIPLIB_BENCHMARKS[name]
        try:
            mps_file = ensure_instance(name, args.data_dir)
        except Exception as exc:
            print(f"[-] {name}: cannot access/download: {exc}")
            all_ok = False
            continue

        res_off = run_solver(solver, mps_file, ["--no-cuts"])
        res_on = run_solver(solver, mps_file, [])

        ref_obj = meta["optimal"]
        obj_off = res_off.get("objective", float("nan"))
        obj_on = res_on.get("objective", float("nan"))
        verified = (
            res_off.get("status") == "Optimal" and res_off.get("verified", False)
            and res_on.get("status") == "Optimal" and res_on.get("verified", False)
            and res_off.get("exit_code") == 0 and res_on.get("exit_code") == 0
        )
        obj_match = False
        if verified:
            obj_match = (
                abs(obj_on - ref_obj) / max(1.0, abs(ref_obj)) <= 1e-4
                and abs(obj_off - ref_obj) / max(1.0, abs(ref_obj)) <= 1e-4
                and abs(obj_on - obj_off) / max(1.0, abs(ref_obj)) <= 1e-4
            )

        nodes_off = res_off.get("nodes_explored", 0)
        nodes_on = res_on.get("nodes_explored", 0)
        reduction = 0.0
        if nodes_off > 0:
            reduction = (1.0 - nodes_on / nodes_off) * 100.0

        row_ok = verified and obj_match
        if not row_ok:
            all_ok = False
        if row_ok and reduction >= args.min_reduction:
            passing += 1

        rows.append({
            "instance": name.upper(),
            "reference_objective": ref_obj,
            "objective_no_cuts": obj_off,
            "objective_cuts": obj_on,
            "nodes_no_cuts": nodes_off,
            "nodes_cuts": nodes_on,
            "node_reduction_pct": round(reduction, 1),
            "cuts_generated": res_on.get("cuts_generated", 0),
            "lp_iterations_no_cuts": res_off.get("lp_iterations", 0),
            "lp_iterations_cuts": res_on.get("lp_iterations", 0),
            "runtime_ms_no_cuts": round(res_off.get("wall_ms", 0.0), 2),
            "runtime_ms_cuts": round(res_on.get("wall_ms", 0.0), 2),
            "verified": bool(row_ok),
            "gate_pass": bool(row_ok and reduction >= args.min_reduction),
        })

        verdict = "PASS" if rows[-1]["gate_pass"] else ("VERIFY-FAIL" if not row_ok else "REDUCTION-LOW")
        print(f"  {name:<10} nodes {nodes_off:>6} -> {nodes_on:>6}  "
              f"reduction {reduction:6.1f}%  cuts {rows[-1]['cuts_generated']:>4}  {verdict}")

    if not rows:
        print("[-] no instances measured", file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    print("=" * 100)
    print(f"[+] results written to {args.output}")
    print(f"[+] instances reaching >= {args.min_reduction:.1f}% reduction: {passing}/{len(rows)}")

    if not all_ok:
        print("[-] verification failed: a run was not Optimal/verified or objectives diverged",
              file=sys.stderr)
        sys.exit(1)
    if passing < args.min_passing:
        print(f"[-] only {passing} instance(s) reached the reduction gate "
              f"(need {args.min_passing})", file=sys.stderr)
        sys.exit(1)
    print("[+] cut effectiveness gate PASSED (R5)")
    sys.exit(0)


if __name__ == "__main__":
    main()
