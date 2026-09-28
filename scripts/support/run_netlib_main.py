from .run_netlib_config import (
    EXTENDED_INSTANCES, NETLIB_BENCHMARKS, OFFLINE_INSTANCES, argparse, csv, os, sys
)
from .run_netlib_run_instance import ensure_instance
from .run_netlib_run_instance import find_solver_binary
from .run_netlib_run_instance import run_instance

def main():
    parser = argparse.ArgumentParser(description="markov-cero Netlib Benchmark Runner")
    parser.add_argument(
        "--solver",
        default=None,
        help="Path to markov-cero-solve executable (default: auto-detected)",
    )
    parser.add_argument(
        "--data-dir",
        default="data/netlib",
        help="Directory to store/read MPS files (default: data/netlib)",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output CSV path (default: netlib_results.csv or netlib_extended.csv)",
    )
    parser.add_argument(
        "--engine",
        choices=["primal", "dual"],
        default="primal",
        help="Simplex engine to test (default: primal)",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=1e-5,
        help="Relative objective tolerance for verification (default: 1e-5)",
    )
    parser.add_argument(
        "--extended",
        action="store_true",
        help="Run extended benchmark set (requires network/large instances)",
    )
    parser.add_argument(
        "--instances",
        nargs="+",
        default=None,
        help="Subset of instances to run",
    )
    args = parser.parse_args()

    if args.instances is not None:
        target_instances = args.instances
        default_output = (
            "evidence/netlib_extended.csv" if args.extended else "evidence/netlib_results.csv"
        )
    elif args.extended:
        target_instances = EXTENDED_INSTANCES
        default_output = "evidence/netlib_extended.csv"
    else:
        target_instances = OFFLINE_INSTANCES
        default_output = "evidence/netlib_results.csv"

    output_path = args.output or default_output

    solver = args.solver or find_solver_binary()
    if not solver:
        print(
            "[-] Error: markov-cero-solve executable not found. Build the project first.",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"=== markov-cero Netlib Benchmark Suite ===")
    print(f"Solver:    {solver}")
    print(f"Engine:    {args.engine}")
    print(f"Data dir:  {args.data_dir}")
    print(f"Output:    {output_path}")
    print(f"Instances: {len(target_instances)}")
    print("=" * 105)

    results = []
    all_passed = True

    header_fmt = "{:<10} {:>8} {:>8} {:>18} {:>18} {:>10} {:>10} {:>10} {:>10}"
    row_fmt = "{:<10} {:>8} {:>8} {:>18.6f} {:>18.6f} {:>10.2e} {:>10} {:>10.1f} {:>10}"

    print(
        header_fmt.format(
            "Instance", "Rows", "Cols", "Ref Objective", "markov-cero Obj", "Rel Error", "Iters", "Time(ms)", "Status"
        )
    )
    print("-" * 105)

    for name in target_instances:
        if name not in NETLIB_BENCHMARKS:
            print(f"[!] Warning: Unknown instance {name}, skipping.")
            continue

        meta = NETLIB_BENCHMARKS[name]
        try:
            mps_file = ensure_instance(name, args.data_dir)
            res = run_instance(solver, mps_file, engine=args.engine)
        except FileNotFoundError as error:
            print(f"[-] {name}: {error}")
            res = {"status": "DatasetUnavailable", "verified": False, "exit_code": -1}
            all_passed = False

        status = res.get("status", "Unknown")
        verified = res.get("verified", False)
        computed_obj = res.get("objective", float("nan"))
        ref_obj = meta["optimal"]

        rel_error = float("nan")
        if isinstance(computed_obj, (int, float)) and status == "Optimal":
            rel_error = abs(computed_obj - ref_obj) / max(1.0, abs(ref_obj))

        iters = res.get("phase_one_iterations", 0) + res.get("phase_two_iterations", 0)
        time_ms = res.get("runtime_ms", res.get("wall_ms", 0.0))

        is_passed = (
            res.get("exit_code") == 0
            and status == "Optimal"
            and verified
            and (rel_error < args.tolerance)
        )

        if not is_passed:
            all_passed = False
            verdict = "FAIL"
        else:
            verdict = "PASS"

        print(
            row_fmt.format(
                name.upper(),
                meta["rows"],
                meta["cols"],
                ref_obj,
                computed_obj,
                rel_error,
                iters,
                time_ms,
                verdict,
            )
        )

        record = {
            "instance": name.upper(),
            "rows": meta["rows"],
            "cols": meta["cols"],
            "reference_objective": ref_obj,
            "computed_objective": computed_obj,
            "relative_error": rel_error,
            "status": status,
            "verified": verified,
            "canonical_verified": res.get("canonical_verified", False),
            "original_verified": res.get("original_verified", False),
            "phase_one_iterations": res.get("phase_one_iterations", 0),
            "phase_two_iterations": res.get("phase_two_iterations", 0),
            "total_iterations": iters,
            "runtime_ms": time_ms,
            "max_primal_violation": res.get("maximum_primal_violation", 0.0),
            "max_dual_violation": res.get("maximum_canonical_dual_violation", 0.0),
            "pass": verdict == "PASS",
        }
        if args.extended or "extended" in output_path:
            record["reproducibility"] = "hash-pinned optional dataset"
        results.append(record)

    print("-" * 105)

    # Save CSV
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(results[0].keys()) if results else ["instance", "status"])
        writer.writeheader()
        writer.writerows(results)

    print(f"\n[+] Results written to {output_path}")
    passed_count = sum(1 for r in results if r["pass"])
    print(f"[+] Summary: {passed_count}/{len(results)} Netlib benchmark problems passed.")

    if not all_passed:
        print("[-] Some benchmark problems failed.", file=sys.stderr)
        sys.exit(1)
    else:
        print("[+] 100% of benchmark problems PASSED with mathematical optimality verification!")
        sys.exit(0)
