from .run_gpu_config import (
    Any, BENCHMARKS, DEFAULT_INSTANCES, Dict, List, RECORD_FIELDS, TABLE_WIDTH, argparse, csv, json, math, os, sys
)
from .run_gpu_run_configuration import configuration_failures
from .run_gpu_detect_hardware import detect_hardware
from .run_gpu_run_configuration import failed_record
from .run_gpu_detect_hardware import find_solver_binary
from .run_gpu_run_configuration import fmt_speedup
from .run_gpu_detect_hardware import print_hardware
from .run_gpu_run_configuration import run_configuration

def main():
    parser = argparse.ArgumentParser(
        description="markov-cero GPU Benchmark Runner (Simplex vs CPU PDLP vs GPU PDLP)"
    )
    parser.add_argument("--solver", help="Path to markov-cero-solve executable")
    parser.add_argument(
        "--instances", nargs="+", default=DEFAULT_INSTANCES, help="Netlib instances to benchmark"
    )
    parser.add_argument("--data-dir", default="data/netlib", help="Directory containing MPS files")
    parser.add_argument(
        "--tolerance", type=float, default=1e-4, help="Relative KKT tolerance for PDLP"
    )
    parser.add_argument("--timeout", type=int, default=120, help="Per-run timeout in seconds")
    parser.add_argument(
        "--max-simplex-rows", type=int, default=0,
        help="Skip simplex baseline if rows exceed this value (0 = no limit)"
    )
    parser.add_argument(
        "--output", default="reports/gpu_benchmark.csv", help="Output path for benchmark CSV"
    )
    parser.add_argument(
        "--hardware-output", default="evidence/gpu_hardware.json",
        help="Path for the machine-readable hardware sidecar (empty string disables)"
    )

    args = parser.parse_args()

    solver = args.solver or find_solver_binary()
    if not solver or not os.access(solver, os.X_OK):
        print(f"Error: Solver executable not found: {solver}", file=sys.stderr)
        sys.exit(1)

    hardware = detect_hardware(solver)
    hw_id = hardware["hardware_id"]

    print("=" * TABLE_WIDTH)
    print(" markov-cero GPU Acceleration Benchmark (D-GPU-08 / Master Document §18.3)")
    print(f" Solver:    {solver}")
    print(f" Tolerance: {args.tolerance:.1e}")
    print(f" Output:    {args.output}")
    print(f" Instances: {', '.join(args.instances)}")
    print_hardware(hardware, args.hardware_output)
    print("=" * TABLE_WIDTH)

    header = (
        "{:<12} {:>5} {:>5} | {:>9} {:>6} | {:>8} {:>6} | "
        "{:>7} {:>7} {:>7} {:>8} | {:>7} | {:<44}"
    )
    print(
        header.format(
            "Instance", "Rows", "Cols",
            "Smplx(ms)", "Iters",
            "CPUp(ms)", "Iters",
            "H2D(ms)", "Krn(ms)", "D2H(ms)", "GPUp(ms)",
            "KrnSpd", "Result"
        )
    )
    print("-" * TABLE_WIDTH)

    records: List[Dict[str, Any]] = []
    all_passed = True

    for inst in args.instances:
        meta = BENCHMARKS.get(inst.lower(), {"rows": 0, "cols": 0, "optimal": 0.0})
        mps_file = os.path.join(args.data_dir, f"{inst.lower()}.mps")
        if not os.path.isfile(mps_file):
            alt = os.path.join("examples", f"{inst.lower()}.mps")
            if os.path.isfile(alt):
                mps_file = alt
            else:
                print(f"[!] Error: Cannot find {mps_file}; marking {inst.upper()} FAILED.",
                      file=sys.stderr)
                records.append(failed_record(
                    inst, meta["rows"], meta["cols"], meta["optimal"],
                    f"file=MPS not found: {mps_file}", hw_id))
                all_passed = False
                continue

        # 1. GPU PDLP
        res_gpu = run_configuration(
            solver, mps_file, engine="pdlp", backend="gpu",
            tolerance=args.tolerance, timeout=args.timeout
        )

        # 2. CPU PDLP
        res_cpu = run_configuration(
            solver, mps_file, engine="pdlp", backend="cpu",
            tolerance=args.tolerance, timeout=args.timeout
        )

        rows = res_gpu["rows"] or res_cpu["rows"] or meta["rows"]
        cols = res_gpu["cols"] or res_cpu["cols"] or meta["cols"]
        nnz = res_gpu["nonzeros"] or res_cpu["nonzeros"] or 0

        # 3. CPU Simplex baseline (optionally skip on massive instances)
        simplex_skipped = args.max_simplex_rows > 0 and rows > args.max_simplex_rows
        if simplex_skipped:
            res_simplex = {
                "exit_code": 0, "status": "Skipped", "verified": False,
                "rows": rows, "cols": cols, "nonzeros": nnz,
                "objective": float("nan"), "iterations": 0, "time_ms": float("nan"),
                "h2d_ms": 0.0, "kernel_ms": 0.0, "d2h_ms": 0.0, "total_ms": float("nan"),
                "error": f"Skipped (> {args.max_simplex_rows} rows)"
            }
        else:
            # Same pre-declared fallback policy as run_compare.py: try dual, then
            # primal, then pdlp. The reported time INCLUDES every failed attempt
            # (total time-to-verified-answer) — no cherry-picking. Rows where all
            # engines fail are reported as failures, never dropped.
            res_simplex = {"status": "NotRun", "verified": False, "time_ms": 0.0}
            for fallback_engine in ("dual", "primal", "pdlp"):
                attempt = run_configuration(
                    solver, mps_file, engine=fallback_engine,
                    tolerance=args.tolerance, timeout=args.timeout
                )
                attempt_time = attempt.get("time_ms") or 0.0
                attempt["time_ms"] = res_simplex["time_ms"] + attempt_time
                if attempt.get("status") == "Optimal" and attempt.get("verified"):
                    res_simplex = attempt
                    break
                res_simplex = attempt

        speedup_kernel = (
            (res_cpu["time_ms"] / res_gpu["kernel_ms"]) if res_gpu["kernel_ms"] > 0 else 0.0
        )
        speedup_end_to_end = (
            (res_cpu["time_ms"] / res_gpu["total_ms"]) if res_gpu["total_ms"] > 0 else 0.0
        )
        smplx_ms = res_simplex["time_ms"]
        speedup_vs_simplex = (
            (smplx_ms / res_gpu["total_ms"])
            if (res_gpu["total_ms"] > 0 and smplx_ms > 0 and not math.isnan(smplx_ms))
            else float("nan")
        )

        # Every configuration must terminate Optimal AND verify; a skipped simplex
        # baseline (by --max-simplex-rows) is explicitly not a failure. Any other
        # non-Optimal status (e.g. BLEND -> NumericalFailure) fails the instance and
        # must reach both the results table and the exit status.
        failures: List[str] = []
        failures += configuration_failures("simplex", res_simplex, allow_skipped=True)
        failures += configuration_failures("cpu_pdlp", res_cpu)
        failures += configuration_failures("gpu_pdlp", res_gpu)
        inst_passed = not failures
        if not inst_passed:
            all_passed = False

        smplx_str = f"{smplx_ms:.2f}" if not math.isnan(smplx_ms) else "N/A"
        if inst_passed:
            result_str = "PASS"
        else:
            result_str = "FAIL (" + "; ".join(failures) + ")"
        row = (
            "{:<12} {:>5} {:>5} | {:>9} {:>6} | {:>8.2f} {:>6} | "
            "{:>7.3f} {:>7.3f} {:>7.3f} {:>8.2f} | {:>6.2f}x | {}"
        ).format(
            inst.upper(), rows, cols,
            smplx_str, res_simplex["iterations"],
            res_cpu["time_ms"], res_cpu["iterations"],
            res_gpu["h2d_ms"], res_gpu["kernel_ms"], res_gpu["d2h_ms"], res_gpu["total_ms"],
            speedup_kernel, result_str
        )
        print(row)
        if not inst_passed:
            print(f"               [-] {inst.upper()} failed: {'; '.join(failures)}",
                  file=sys.stderr)

        ref_obj = meta["optimal"] if meta["optimal"] != 0.0 else res_gpu["objective"]
        record = {
            "instance": inst.upper(),
            "rows": rows,
            "cols": cols,
            "nonzeros": nnz,
            "reference_objective": ref_obj,
            "simplex_status": res_simplex["status"],
            "simplex_verified": res_simplex["verified"],
            "simplex_objective": res_simplex["objective"],
            "simplex_iterations": res_simplex["iterations"],
            "simplex_time_ms": res_simplex["time_ms"],
            "cpu_pdlp_status": res_cpu["status"],
            "cpu_pdlp_verified": res_cpu["verified"],
            "cpu_pdlp_objective": res_cpu["objective"],
            "cpu_pdlp_iterations": res_cpu["iterations"],
            "cpu_pdlp_time_ms": res_cpu["time_ms"],
            "gpu_pdlp_status": res_gpu["status"],
            "gpu_pdlp_objective": res_gpu["objective"],
            "gpu_pdlp_iterations": res_gpu["iterations"],
            "gpu_h2d_ms": res_gpu["h2d_ms"],
            "gpu_kernel_ms": res_gpu["kernel_ms"],
            "gpu_d2h_ms": res_gpu["d2h_ms"],
            "gpu_total_ms": res_gpu["total_ms"],
            "speedup_kernel": speedup_kernel,
            "speedup_end_to_end": speedup_end_to_end,
            "speedup_vs_simplex": speedup_vs_simplex,
            "gpu_verified": res_gpu["verified"],
            "failures": "; ".join(failures),
            "hardware_id": hw_id,
            "pass": inst_passed,
        }
        records.append(record)

    print("-" * TABLE_WIDTH)

    # Per-scale verdict table: every problem size is reported individually so a
    # regression on one scale cannot hide behind an aggregate.
    if records:
        print("Per-scale results (one row per problem size, no aggregation):")
        scale_header = (
            "{:<12} {:>6} | {:<17} {:<10} {:<10} | {:>6} | {:>9} | {}"
        )
        print(scale_header.format(
            "Instance", "Rows", "Simplex", "CPU-PDLP", "GPU-PDLP",
            "GPUver", "CPU/GPU", "Verdict"))
        for rec in records:
            verdict = "PASS" if rec["pass"] else f"FAIL: {rec['failures']}"
            gpuver = "yes" if rec["gpu_verified"] else "no"
            print(scale_header.format(
                rec["instance"], rec["rows"],
                str(rec["simplex_status"]), str(rec["cpu_pdlp_status"]),
                str(rec["gpu_pdlp_status"]), gpuver,
                fmt_speedup(rec["speedup_end_to_end"]), verdict))
        passed = sum(1 for r in records if r["pass"])
        print(f" Summary: {passed}/{len(records)} instance(s) passed, "
              f"{len(records) - passed} failed.")

    if records:
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        with open(args.output, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=RECORD_FIELDS)
            writer.writeheader()
            writer.writerows(records)
        print(f"\n[+] Results successfully written to {args.output}")

    if args.hardware_output:
        os.makedirs(os.path.dirname(os.path.abspath(args.hardware_output)), exist_ok=True)
        sidecar = dict(hardware)
        sidecar["run"] = {
            "solver": solver,
            "cwd": os.getcwd(),
            "instances": args.instances,
            "tolerance": args.tolerance,
            "timeout_s": args.timeout,
            "max_simplex_rows": args.max_simplex_rows,
            "output_csv": args.output,
        }
        sidecar["results"] = [
            {"instance": r["instance"], "pass": r["pass"], "failures": r["failures"]}
            for r in records
        ]
        with open(args.hardware_output, "w") as f:
            json.dump(sidecar, f, indent=2)
            f.write("\n")
        print(f"[+] Hardware sidecar written to {args.hardware_output}")

    if not records:
        print("[-] No results recorded: no selected instance produced output.",
              file=sys.stderr)
        sys.exit(1)

    if all_passed:
        print("[+] All selected GPU benchmark problems converged and verified.")
        sys.exit(0)
    else:
        failed = [r for r in records if not r["pass"]]
        print("[-] Failed instances:", file=sys.stderr)
        for r in failed:
            print(f"    {r['instance']}: {r['failures']}", file=sys.stderr)
        sys.exit(1)
