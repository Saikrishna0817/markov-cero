#!/usr/bin/env python3
"""W8: full benchmark runner — markov-cero over every local suite.

Runs the solver over Netlib, MIPLIB, Mittelmann and QPLIB instances and emits
validated CSV records:

  evidence/benchmarks/full_netlib.csv
  evidence/benchmarks/full_miplib.csv
  evidence/benchmarks/full_mittelmann.csv
  evidence/benchmarks/full_qplib.csv

Each row records the instance class and dimensions, solve status, independent
verification, objective error when a reference objective exists, runtime,
nodes, gap and available primal/dual residuals. Missing certificate fields
remain blank; the runner never manufactures them.
"""

import argparse
import csv
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

SUITES = {
    "netlib": {
        "dir": "data/netlib",
        "engine": "auto",
        "glob": "*.mps",
    },
    "miplib": {
        "dir": "data/miplib",
        "engine": "auto",
        "glob": "*.mps",
    },
    "mittelmann": {
        "dir": "data/mittelmann",
        "engine": "auto",
        "glob": "*.mps",
    },
    "qplib": {
        "dir": "data/qp",
        "engine": "auto",
        "glob": "QPLIB_*.mps",
    },
    "cases": {
        "dir": "data/cases",
        "engine": "auto",
        "glob": "*.mps",
    },
}


def qplib_convexity_gate(mps: Path) -> tuple[bool, str]:
    """Admit convex continuous QPs with linear or box constraints only.

    QPLIB's first class letter C/D guarantees a convex quadratic objective.
    Q-coded objectives are only admitted after an explicit dense PSD check;
    this is limited to small matrices so an unverified large model is never
    silently called convex. The third class letter must describe linear or
    bound-only constraints, which is the subset this QP engine supports.
    """
    provenance_path = mps.with_suffix(".provenance.json")
    try:
        provenance = json.loads(provenance_path.read_text())
        code = str(provenance.get("problem_class", ""))
        n = int(provenance.get("columns", 0))
        direction = str(provenance.get("direction", "minimize")).lower()
    except (OSError, ValueError, json.JSONDecodeError):
        return False, "missing or invalid QPLIB provenance/class code"
    if len(code) != 3 or code[1] != "C":
        return False, f"variable class {code[1:2]!r} is not continuous"
    if code[2] not in {"L", "B"}:
        return False, f"constraint class {code!r} is outside linear/bounds-only QP scope"
    if code[0] in {"C", "D"}:
        return True, f"QPLIB {code} convex-objective class with supported constraints"
    if code[0] != "Q":
        return False, f"unsupported objective class {code!r}"
    if n <= 0 or n > 1024:
        return False, f"Q-coded objective dimension {n} exceeds explicit PSD-check limit"

    import numpy as np
    from scripts.check_qp_convention import read_quadratics

    _, entries = read_quadratics(str(mps))
    hessian = np.zeros((n, n), dtype=np.float64)
    for _, col_i, col_j, value in entries:
        try:
            i, j = int(col_i[1:]) - 1, int(col_j[1:]) - 1
        except (ValueError, IndexError):
            return False, "cannot map QPLIB quadratic variable names for PSD check"
        if not (0 <= i < n and 0 <= j < n):
            return False, "quadratic variable index is outside provenance dimensions"
        hessian[i, j] += value
        if i != j:
            hessian[j, i] += value
    if direction == "maximize":
        hessian *= -1.0
    eigenvalues = np.linalg.eigvalsh(hessian)
    scale = max(1.0, float(np.max(np.abs(eigenvalues))))
    if float(eigenvalues[0]) < -1e-10 * scale:
        return False, f"objective Hessian is not PSD (minimum eigenvalue {eigenvalues[0]:.6g})"
    return True, f"Q-coded objective passed dense PSD check (min eigenvalue {eigenvalues[0]:.6g})"


def run_one(solver: str, mps: Path, suite: str, engine: str,
            timeout: float, threads: int, expected_sha256: str = "") -> dict:
    provenance_path = REPO / "data" / suite / f"{mps.stem}.provenance.json"
    best_known = ""
    if provenance_path.is_file():
        try:
            reference = json.loads(provenance_path.read_text()).get("reference_objective")
            if isinstance(reference, (int, float)):
                best_known = float(reference)
        except (OSError, json.JSONDecodeError):
            pass
    cmd = [solver, str(mps)]
    if engine != "auto":
        cmd += ["--engine", engine]
    cmd += ["--time-limit", str(timeout), "--threads", str(threads)]
    started = time.time()
    row = {
        "solver": "markov-cero",
        "solver_sha256": hashlib.sha256(Path(solver).read_bytes()).hexdigest(),
        "suite": suite,
        "instance": mps.stem,
        "problem_class": "",
        "n_vars": "",
        "n_constraints": "",
        "n_nonzeros": "",
        "status": "Error",
        "verified": False,
        "objective": "",
        "best_known_obj": best_known,
        "obj_error": "",
        "time_ms": 0.0,
        "nodes": "",
        "gap": "",
        "kkt_primal": "",
        "kkt_dual": "",
    }
    if expected_sha256 and row["solver_sha256"].lower() != expected_sha256.lower():
        row["status"] = "BinaryChanged"
        return row
    try:
        # The CLI/API deadline is the solve cap. Keep five seconds for the
        # process to unwind and serialize its ResourceLimit result; this parent
        # watchdog only catches non-cooperative or stuck code paths.
        proc = subprocess.run(cmd, capture_output=True, timeout=timeout + 5.0)
        row["time_ms"] = round((time.time() - started) * 1000.0, 1)
        payload = None
        for line in reversed(proc.stdout.decode("utf-8", "replace").splitlines()):
            try:
                candidate = json.loads(line)
                if isinstance(candidate, dict) and "status" in candidate:
                    payload = candidate
                    break
            except json.JSONDecodeError:
                continue
        if payload is not None:
            row["status"] = payload.get("status", "?")
            row["verified"] = bool(payload.get("verified", False))
            primal = payload.get("primal")
            objective = payload.get("objective", "")
            has_primal = isinstance(primal, list) and len(primal) > 0
            row["objective"] = (objective if row["status"] == "Optimal" or has_primal
                                else "")
            row["problem_class"] = payload.get("problem_class", "")
            row["n_vars"] = payload.get("cols", "")
            row["n_constraints"] = payload.get("rows", "")
            row["n_nonzeros"] = payload.get("nonzeros", "")
            row["nodes"] = payload.get("nodes_explored", "")
            if row["status"] == "Optimal" and row["verified"]:
                row["gap"] = payload.get("relative_gap", "")
                row["kkt_primal"] = payload.get("maximum_canonical_primal_violation", "")
                row["kkt_dual"] = payload.get("maximum_canonical_dual_violation", "")
            if (row["status"] == "Optimal" and row["verified"]
                    and best_known != "" and isinstance(row["objective"], (int, float))):
                row["obj_error"] = abs(float(row["objective"]) - best_known) / max(1.0, abs(best_known))
        else:
            row["status"] = f"Exit{proc.returncode}"
    except subprocess.TimeoutExpired:
        row["time_ms"] = round((time.time() - started) * 1000.0, 1)
        row["status"] = "Timeout"
    except json.JSONDecodeError:
        row["status"] = "BadJSON"
    return row


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--solver", default=str(REPO / "build_w5" / "markov-cero-solve"))
    ap.add_argument("--suites", nargs="+", default=list(SUITES))
    ap.add_argument("--timeout", type=float, default=120.0)
    ap.add_argument("--threads", type=int, default=1,
                    help="solver threads per instance (default: 1)")
    ap.add_argument("--expected-solver-sha256", default="",
                    help="refuse to start unless the executable SHA-256 matches this value")
    ap.add_argument("--resume", action="store_true",
                    help="append missing instances to a prefix of rows from the same solver build; "
                         "use the same timeout and thread count as the original run")
    ap.add_argument("--out-dir", default=str(REPO / "evidence" / "benchmarks"))
    args = ap.parse_args()

    if not Path(args.solver).exists():
        print(f"solver not found: {args.solver}", file=sys.stderr)
        return 2
    executable_sha256 = hashlib.sha256(Path(args.solver).read_bytes()).hexdigest()
    if args.expected_solver_sha256 and executable_sha256.lower() != \
            args.expected_solver_sha256.strip().lower():
        print(f"solver SHA-256 mismatch: expected {args.expected_solver_sha256}, "
              f"found {executable_sha256}", file=sys.stderr)
        return 2
    print(f"W8 executable SHA-256: {executable_sha256}")

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    columns = ["solver", "solver_sha256", "suite", "instance", "problem_class", "n_vars",
               "n_constraints", "n_nonzeros", "status", "verified", "objective",
               "best_known_obj", "obj_error", "time_ms", "nodes", "gap",
               "kkt_primal", "kkt_dual"]
    overall_fail = False

    for suite in args.suites:
        spec = SUITES.get(suite)
        if spec is None:
            print(f"unknown suite: {suite}", file=sys.stderr)
            continue
        suite_dir = REPO / spec["dir"]
        instances = sorted(suite_dir.glob(spec["glob"]))
        excluded = []
        if suite == "qplib":
            eligible = []
            selection_rows = []
            for mps in instances:
                include, reason = qplib_convexity_gate(mps)
                selection_rows.append({"instance": mps.stem,
                                       "included": include, "reason": reason})
                if include:
                    eligible.append(mps)
                else:
                    excluded.append({"instance": mps.stem, "reason": reason})
            instances = eligible
            selection_path = out_dir / "qplib_convexity_selection.csv"
            with selection_path.open("w", newline="") as selection_file:
                selection_writer = csv.DictWriter(
                    selection_file, fieldnames=["instance", "included", "reason"])
                selection_writer.writeheader()
                selection_writer.writerows(selection_rows)
            excluded_path = out_dir / "qplib_excluded.csv"
            with excluded_path.open("w", newline="") as excluded_file:
                excluded_writer = csv.DictWriter(excluded_file, fieldnames=["instance", "reason"])
                excluded_writer.writeheader()
                excluded_writer.writerows(excluded)
        # Keep only true instances (skip provenance JSON etc. via glob).
        out_path = out_dir / f"full_{suite}.csv"
        statuses = {}
        completed = []
        if args.resume and out_path.exists():
            with out_path.open(newline="") as previous:
                reader = csv.DictReader(previous)
                if reader.fieldnames != columns:
                    print(f"cannot resume {out_path}: CSV columns differ", file=sys.stderr)
                    return 2
                completed = list(reader)
            expected_names = [mps.stem for mps in instances[:len(completed)]]
            if ([row["instance"] for row in completed] != expected_names or
                    any(row["suite"] != suite or
                        row["solver_sha256"] != executable_sha256 for row in completed)):
                print(f"cannot resume {out_path}: instance order, suite, or solver hash differs",
                      file=sys.stderr)
                return 2
            for row in completed:
                statuses[row["status"]] = statuses.get(row["status"], 0) + 1
            print(f"[{suite}] resuming after {len(completed)} verified-prefix rows", flush=True)
        with open(out_path, "a" if completed else "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=columns)
            if not completed:
                writer.writeheader()
            for mps in instances[len(completed):]:
                row = run_one(args.solver, mps, suite, spec["engine"],
                              args.timeout, args.threads, executable_sha256)
                writer.writerow(row)
                f.flush()  # Keep long all-suite sweeps resumable and auditable.
                statuses[row["status"]] = statuses.get(row["status"], 0) + 1
                print(f"  {suite}/{row['instance']:<22} {row['status']:<16} "
                      f"verified={row['verified']} {row['time_ms']} ms", flush=True)
        total = sum(statuses.values())
        optimal = statuses.get("Optimal", 0)
        print(f"[{suite}] {total} instances -> {out_path} | "
              f"Optimal {optimal}/{total} | {statuses}")
        if suite == "qplib":
            print(f"[qplib] convex supported subset {total}; excluded {len(excluded)} "
                  f"with reasons -> {out_dir / 'qplib_excluded.csv'}")
        if optimal < total:
            overall_fail = True  # honest reporting; not a hard failure

    return 0 if not overall_fail else 0  # exit 0: report generated regardless


if __name__ == "__main__":
    sys.exit(main())
