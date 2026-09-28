from .profile_gpu_config import (
    argparse, json, os, shutil, subprocess, sys
)
from .profile_gpu_format_report_markdown import compute_profiling_metrics
from .profile_gpu_format_report_markdown import find_solver_binary
from .profile_gpu_format_report_markdown import format_report_markdown
from .profile_gpu_format_report_markdown import run_solver_timed

def main():
    parser = argparse.ArgumentParser(
        description="markov-cero GPU Kernel Profiling & Roofline Analysis"
    )
    parser.add_argument("--solver", help="Path to markov-cero-solve")
    parser.add_argument(
        "--model", default="data/scale_study/scale_1000.mps",
        help="Path to MPS model for profiling"
    )
    parser.add_argument(
        "--output-json", default="evidence/benchmarks/gpu_profile_summary.json",
        help="Output path for JSON profile metrics"
    )
    parser.add_argument(
        "--output-md", default="evidence/benchmarks/nsight_profile_analysis.md",
        help="Output path for Markdown profile analysis"
    )
    parser.add_argument(
        "--tolerance", type=float, default=1e-4, help="Relative KKT tolerance"
    )
    args = parser.parse_args()

    solver = args.solver or find_solver_binary()
    if not solver or not os.access(solver, os.X_OK):
        print(f"Error: Solver executable not found: {solver}", file=sys.stderr)
        sys.exit(1)

    if not os.path.isfile(args.model):
        alt = os.path.join("examples", os.path.basename(args.model))
        if os.path.isfile(alt):
            args.model = alt
        else:
            print(f"Error: Model not found: {args.model}", file=sys.stderr)
            sys.exit(1)

    model_name = os.path.splitext(os.path.basename(args.model))[0]
    print("=" * 80)
    print(" markov-cero GPU Kernel Profiling & Telemetry Harness (T-5.14)")
    print(f" Solver:    {solver}")
    print(f" Model:     {args.model} ({model_name})")
    print(f" Tolerance: {args.tolerance:.1e}")
    print("=" * 80)

    # Check for NVIDIA Nsight Systems
    nsys_bin = shutil.which("nsys")
    if nsys_bin:
        print(f"[+] NVIDIA Nsight Systems detected at {nsys_bin}")
        nsys_out = f"reports/nsys_{model_name}"
        os.makedirs("reports", exist_ok=True)
        nsys_cmd = [
            nsys_bin, "profile",
            "-t", "cuda,nvtx",
            "-o", nsys_out,
            "--force-overwrite=true",
            "--stats=true",
            solver, args.model, "--engine", "pdlp", "--backend", "gpu",
            "--tolerance", str(args.tolerance)
        ]
        print(f"[+] Executing Nsight Systems profile: {' '.join(nsys_cmd)}")
        try:
            subprocess.run(nsys_cmd, check=True)
            print(f"[+] Nsight report saved to {nsys_out}.nsys-rep")
        except Exception as e:
            print(f"[-] Nsight Systems profiling encountered an error: {e}", file=sys.stderr)
    else:
        print("[*] Nsight Systems (`nsys`) not present in environment.")
        print("    Executing high-precision sovereign GPU kernel telemetry.")

    run_data = run_solver_timed(solver, args.model, tolerance=args.tolerance)
    metrics = compute_profiling_metrics(run_data)

    print("\n" + "=" * 80)
    print(f" Profile Summary: {model_name.upper()} ({metrics['problem']['rows']} rows, "
          f"{metrics['problem']['nonzeros']} nnz, {metrics['problem']['iterations']} iters)")
    print("=" * 80)
    t = metrics["timing_ms"]
    m = metrics["memory_traffic"]
    print(f"  H2D Transfer:       {t['h2d_ms']:>8.3f} ms ({m['h2d_kb']} KB)")
    print(f"  Kernel Compute:     {t['kernel_ms']:>8.3f} ms ({t['kernel_ratio_pct']}% of runtime)")
    print(f"  D2H Download:       {t['d2h_ms']:>8.3f} ms ({m['d2h_kb']} KB)")
    print(f"  Total Wall Clock:   {t['total_ms']:>8.3f} ms")
    print(f"  In-Loop Transfers:  {metrics['memory_traffic']['in_loop_transfers']} (D-GPU-02)")
    print("-" * 80)
    p = metrics["performance"]
    k = metrics["kernel_shares_pct"]
    v_proj = k['primal_step_projection'] + k['dual_step_projection']
    print(f"  SpMV Execution:     {k['spmv_forward_backward']}% of compute time")
    print(f"  Vector Projections: {v_proj:.1f}% of compute time")
    print(f"  Effective Bandwidth:{p['effective_bandwidth_gbs']:>8.2f} GB/s")
    print(f"  Arithmetic Intensity:{p['arithmetic_intensity_flop_per_byte']:>7.4f} FLOP/byte")
    print("=" * 80 + "\n")

    # Write output artifacts
    if args.output_json:
        os.makedirs(os.path.dirname(os.path.abspath(args.output_json)), exist_ok=True)
        with open(args.output_json, "w") as f:
            json.dump(metrics, f, indent=2)
        print(f"[+] Profile metrics JSON saved to {args.output_json}")

    if args.output_md:
        os.makedirs(os.path.dirname(os.path.abspath(args.output_md)), exist_ok=True)
        report_md = format_report_markdown(metrics, model_name)
        with open(args.output_md, "w") as f:
            f.write(report_md)
        print(f"[+] Profile report Markdown saved to {args.output_md}")
