from .run_full_benchmark_config import (
    Path, REPO, SUITES, argparse, csv, hashlib, sys, json, suite_time_limit
)
from .run_full_benchmark_run_one import qplib_convexity_gate
from .run_full_benchmark_run_one import run_one

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--solver", default=str(REPO / "build_w5" / "markov-cero-solve"))
    ap.add_argument("--suites", nargs="+", default=list(SUITES))
    ap.add_argument("--timeout", type=float, default=None,
                    help="per-instance time cap in seconds (frozen default: the "
                         "suite cap — LP/QP 60 s, MILP 300 s)")
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
        # The full-suite denominator includes archived optional instances.
        manifest = json.loads((REPO / "data/optional-datasets.json").read_text())
        optional = [REPO / entry["path"] for entry in manifest
                    if Path(entry["path"]).parent == Path(spec["dir"])
                    and Path(entry["path"]).match(spec["glob"])]
        instances = sorted(set(suite_dir.glob(spec["glob"])) | set(optional))
        excluded = []
        if suite == "qplib":
            eligible = []
            selection_rows = []
            for mps in instances:
                if not mps.exists():
                    eligible.append(mps)
                    continue
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
        timeout = (args.timeout if args.timeout is not None
                   else suite_time_limit(suite))
        with open(out_path, "a" if completed else "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=columns)
            if not completed:
                writer.writeheader()
            for mps in instances[len(completed):]:
                row = run_one(args.solver, mps, suite, spec["engine"],
                              timeout, args.threads, executable_sha256)
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

    return 1 if overall_fail else 0
