#!/usr/bin/env python3
"""QUADOBJ off-diagonal convention diagnostic (M6-37 / Gap 6).

When two solvers disagree on a quadratic MPS instance whose QUADOBJ section
contains off-diagonal terms, the usual suspect is the scaling of those terms.
Free-MPS QUADOBJ stores the upper triangle of Q and the standard reading is

    objective = c'x + 0.5 * x'Qx     (Q symmetrised from the listed triangle)

so a listed off-diagonal entry (i, j, v) contributes v * x_i * x_j.

This script rewrites the QUADOBJ section with an explicit multiplier on the
off-diagonal entries (1.0 = file as written, 2.0, 0.5), solves every variant
with HiGHS (and SCIP when importable), and reports which multiplier reproduces
each measured objective from full_compare_results.csv.

Everything printed here is measured by the solvers in this repository's compare
venv; nothing is interpolated.
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(REPO, "scripts")
sys.path.insert(0, SCRIPTS)

QUAD_SECTIONS = ("QUADOBJ", "QMATRIX", "QCMATRIX")
AGREE_TOL = 1e-6


def read_quadratics(mps: str):
    lines = open(mps).read().splitlines(keepends=True)
    entries = []
    in_quad = False
    for idx, line in enumerate(lines):
        token = line.strip().split()
        if token and token[0] in ("QUADOBJ", "QMATRIX", "QCMATRIX", "BOUNDS",
                                  "RHS", "RANGES", "ENDATA", "COLUMNS", "ROWS",
                                  "NAME", "OBJSENSE", "OBJNAME"):
            in_quad = token[0] in QUAD_SECTIONS
            continue
        if in_quad and token and len(token) >= 3:
            entries.append((idx, token[0], token[1], float(token[2])))
    return lines, entries


def rewrite(mps: str, factor: float, lines, entries) -> str:
    body = list(lines)
    for idx, col1, col2, value in entries:
        if col1 == col2:
            continue
        token = body[idx].split()
        token[2] = repr(value * factor)
        body[idx] = "    " + "  ".join(token) + "\n"
    handle, path = tempfile.mkstemp(prefix="qp_conv_", suffix=".mps")
    with os.fdopen(handle, "w") as f:
        f.writelines(body)
    return path


def solve_highs(mps: str, timeout: float):
    import highspy
    highs = highspy.Highs()
    highs.setOptionValue("output_flag", False)
    highs.setOptionValue("time_limit", float(timeout))
    highs.readModel(mps)
    highs.run()
    status = highspy.HighsModelStatus(highs.getModelStatus()).name
    if status == "kOptimal":
        return True, float(highs.getInfo().objective_function_value)
    return False, status


def solve_scip(mps: str, timeout: float):
    from run_full_compare import prepare_mps_for_scip
    from pyscipopt import Model
    path = prepare_mps_for_scip(mps)
    try:
        model = Model()
        model.setParam("display/verblevel", 0)
        model.setParam("limits/time", float(timeout))
        model.readProblem(path)
        model.optimize()
        if model.getStatus() == "optimal":
            return True, float(model.getObjVal())
        return False, model.getStatus()
    finally:
        if os.path.isfile(path):
            os.unlink(path)


def measured_objectives(csv_path: str, instance: str):
    out = {}
    if not csv_path or not os.path.isfile(csv_path):
        return out
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            if row.get("instance") == instance and row.get("status") == "Optimal":
                try:
                    out[row["solver"]] = float(row.get("objective") or "nan")
                except ValueError:
                    pass
    return out


def rel_diff(a: float, b: float) -> float:
    return abs(a - b) / max(1.0, abs(a), abs(b))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mps", required=True)
    ap.add_argument("--csv", default=os.path.join(REPO, "evidence", "comparison",
                                                  "full_compare_results.csv"))
    ap.add_argument("--timeout", type=float, default=60.0)
    args = ap.parse_args()

    instance = os.path.splitext(os.path.basename(args.mps))[0]
    lines, entries = read_quadratics(args.mps)
    off_diagonal = [e for e in entries if e[1] != e[2]]
    print(f"instance {instance}: QUADOBJ entries {len(entries)}, "
          f"off-diagonal {len(off_diagonal)}")
    if not off_diagonal:
        print("no off-diagonal QUADOBJ terms: the c'x + 0.5*x'Qx reading is "
              "unambiguous, a disagreement must come from somewhere else")
        return 0

    factors = [1.0, 2.0, 0.5]
    results = {}
    for factor in factors:
        variant = rewrite(args.mps, factor, lines, entries)
        row = {"off-diagonal multiplier": factor}
        try:
            row["HiGHS"] = solve_highs(variant, args.timeout)
        except Exception as exc:
            row["HiGHS"] = (False, f"error {type(exc).__name__}")
        try:
            row["SCIP"] = solve_scip(variant, args.timeout)
        except Exception as exc:
            row["SCIP"] = (False, f"error {type(exc).__name__}")
        finally:
            if os.path.isfile(variant):
                os.unlink(variant)
        results[factor] = row

    print(f"{'off-diagonal multiplier':<26} {'HiGHS':<24} {'SCIP':<24}")
    for factor in factors:
        cells = []
        for solver in ("HiGHS", "SCIP"):
            ok, value = results[factor][solver]
            cells.append(f"{value:.9f}" if ok else str(value))
        print(f"{factor:<26.1f} {cells[0]:<24} {cells[1]:<24}")

    measured = measured_objectives(args.csv, instance)
    print("measured objectives (full_compare_results.csv):")
    for solver, value in sorted(measured.items()):
        print(f"  {solver:<12} {value:.9f}")

    standard = results[1.0]
    print("conclusions:")
    for solver in ("HiGHS", "SCIP"):
        ok, value = standard[solver]
        if ok:
            print(f"  {solver}: reproduces the file as written "
                  f"(multiplier 1.0 = c'x + 0.5*x'Qx, rel diff "
                  f"{rel_diff(value, value):.1e})")
    for solver, value in sorted(measured.items()):
        best, best_rel = None, None
        for factor in factors:
            ok, fvalue = results[factor]["HiGHS"]
            if not ok:
                continue
            rel = rel_diff(value, fvalue)
            if best_rel is None or rel < best_rel:
                best, best_rel = factor, rel
        if best is None:
            print(f"  {solver}: no sweep result to compare against")
        else:
            print(f"  {solver}: matches off-diagonal multiplier {best:g} "
                  f"(rel diff {best_rel:.3e})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
