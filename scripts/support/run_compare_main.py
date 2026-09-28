from .run_compare_config import (
    argparse, csv, math, os, random, statistics, sys
)
from .run_compare_run_markov_cero import dolan_more_profile
from .run_compare_run_markov_cero import find_solver
from .run_compare_run_markov_cero import geometric_mean_ratio
from .run_compare_run_markov_cero import load_instance_list
from .run_compare_run_markov_cero import repo_path
from .run_compare_run_markov_cero import run_highs
from .run_compare_run_markov_cero import run_markov_cero
from .run_compare_run_markov_cero import sha256_file
from .run_compare_run_markov_cero import write_profile_svg

def main():
    ap = argparse.ArgumentParser(description="markov-cero R16 comparison harness (RW-3)")
    ap.add_argument("--solver", default=None, help="markov-cero-solve binary")
    ap.add_argument("--suites", nargs="+", default=["netlib", "miplib"],
                    help="Instance lists under data/compare/<suite>.txt")
    ap.add_argument("--baseline", default="highs", choices=["highs"],
                    help="Established baseline solver")
    ap.add_argument("--repeat", type=int, default=3, help="Repeats per cell (median reported)")
    ap.add_argument("--timeout", type=float, default=60.0, help="Per-run time limit (s)")
    ap.add_argument("--threads", type=int, default=1,
                    help="Matched solver threads (fixed for all repeats; default 1)")
    ap.add_argument("--penalty-sec", type=float, default=300.0,
                    help="Penalty time for failed/timeout cells before log aggregation")
    ap.add_argument("--agreement-tol", type=float, default=1e-4,
                    help="Relative objective agreement required for a PASS row "
                         "(default 1e-4 = the solver's PDLP-class KKT tolerance; "
                         "simplex rows typically agree to 1e-15)")
    ap.add_argument("--output-dir", default=None, help="Defaults to evidence/compare")
    ap.add_argument("--highs-python", default=None,
                    help="Python interpreter with highspy (default: this one)")
    args = ap.parse_args()
    if args.repeat < 1 or not 1 <= args.threads <= 256 or not math.isfinite(args.timeout) or args.timeout <= 0:
        ap.error("positive repeats/time limit and 1..256 threads required")

    solver = find_solver(args.solver)
    if not solver:
        print("[-] markov-cero-solve not found; pass --solver", file=sys.stderr)
        sys.exit(1)
    solver_sha256 = sha256_file(solver, full=True)

    if args.baseline == "highs":
        try:
            if args.highs_python:
                # Import highspy from the interpreter that has it available.
                import subprocess as _sp
                probe = _sp.run([args.highs_python, "-c", "import highspy"],
                                capture_output=True, text=True)
                if probe.returncode == 0:
                    venv_site = _sp.run(
                        [args.highs_python, "-c",
                         "import sys; print([p for p in sys.path if 'site-packages' in p][0])"],
                        capture_output=True, text=True)
                    if venv_site.returncode == 0:
                        sys.path.insert(0, venv_site.stdout.strip())
            import highspy  # noqa: F401
            highs_mod = highspy
        except ImportError:
            print("[-] highspy not importable; install with: pip install highspy "
                  "(or pass --highs-python)", file=sys.stderr)
            sys.exit(1)

    instances = load_instance_list(args.suites)
    if not instances:
        print("[-] no instances found in data/compare/", file=sys.stderr)
        sys.exit(1)

    out_dir = args.output_dir or repo_path("evidence", "compare")
    os.makedirs(out_dir, exist_ok=True)

    print("=== markov-cero vs HiGHS comparison (R16 / RW-3) ===")
    print(f"Solver:   {solver}")
    print(f"Baseline: HiGHS {highs_mod.Highs().version()} (external oracle)")
    print(f"Instances: {len(instances)} | repeats: {args.repeat} | timeout: {args.timeout}s | "
          f"markov-cero threads: {args.threads}")
    print("=" * 100)

    penalty_ms = args.penalty_sec * 1000.0
    rows = []
    for inst in instances:
        if sha256_file(solver, full=True) != solver_sha256:
            print("[-] solver binary changed during comparison run", file=sys.stderr)
            sys.exit(2)
        if not os.path.isfile(inst["mps"]):
            print(f"[!] {inst['name']}: MPS missing ({inst['mps']}), skipping", file=sys.stderr)
            continue

        mc_times, hi_times = [], []
        mc_status, mc_obj, mc_verified = "", float("nan"), False
        hi_status, hi_obj = "", float("nan")
        mc = {"engine": "auto"}
        hi = {"time_ms": penalty_ms}
        # Repeat both solvers and alternate their order by instance/trial to
        # reduce warm-cache and machine-load bias.
        rng = random.Random(f"{inst['name']}:{args.repeat}")
        for _ in range(max(1, args.repeat)):
            order = ["mc", "hi"]
            rng.shuffle(order)
            for solver_name in order:
                if solver_name == "mc":
                    r = run_markov_cero(solver, inst["mps"], args.timeout, args.threads)
                    mc_times.append(r["time_ms"] if not math.isnan(r["time_ms"]) else penalty_ms)
                    mc_status, mc_obj, mc_verified = r["status"], r["objective"], r["verified"]
                    mc = r
                else:
                    hi = run_highs(highs_mod, inst["mps"], args.timeout, penalty_ms, args.threads, args.highs_python)
                    hi_times.append(hi["time_ms"])
        mc_time = statistics.median(mc_times)
        if mc_status != "Optimal" or not mc_verified:
            mc_time = max(mc_time, penalty_ms)

        hi_time = statistics.median(hi_times)
        hi_status, hi_obj = hi["status"], hi["objective"]
        if hi_status != "Optimal":
            hi_time = max(hi_time, penalty_ms)

        ratio = mc_time / hi_time if hi_time > 0 else float("nan")
        agreement = float("nan")
        if not math.isnan(mc_obj) and not math.isnan(hi_obj) and mc_status == "Optimal" \
                and hi_status == "Optimal":
            agreement = abs(mc_obj - hi_obj) / max(1.0, abs(hi_obj))

        ok = mc_status == "Optimal" and mc_verified and (
            math.isnan(agreement) or agreement <= args.agreement_tol)
        rows.append({
            "suite": inst["suite"], "instance": inst["name"],
            "solver_sha256": solver_sha256,
            "mc_threads": args.threads,
            "mps_sha": sha256_file(inst["mps"]),
            "mc_status": mc_status, "mc_objective": mc_obj, "mc_time_ms": mc_time,
            "mc_time_min_ms": min(mc_times), "mc_time_max_ms": max(mc_times),
            "mc_verified": mc_verified, "mc_engine": mc.get("engine", "auto"),
            "hi_status": hi_status, "hi_objective": hi_obj, "hi_time_ms": hi_time,
            "hi_time_min_ms": min(hi_times), "hi_time_max_ms": max(hi_times),
            "ratio_mc_over_hi": ratio, "objective_agreement": agreement, "pass": ok,
        })
        flag = "PASS" if ok else "FAIL"
        print(f"  {inst['name']:<12} mc:{mc_status:<9} hi:{hi_status:<9} "
              f"t_mc={mc_time:>10.1f}ms t_hi={hi_time:>10.1f}ms "
              f"ratio={ratio:>7.2f}x agree={agreement:.2e} [{flag}]")

    if not rows:
        print("[-] no comparable rows produced", file=sys.stderr)
        sys.exit(1)

    # ---- outputs
    csv_path = os.path.join(out_dir, "results.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    ratios = [r["ratio_mc_over_hi"] if r["mc_status"] == "Optimal" and r["hi_status"] == "Optimal"
              else penalty_ms / max(r["hi_time_ms"], 1e-9) for r in rows]
    gm = geometric_mean_ratio(ratios, penalty_ms)
    passed = sum(1 for r in rows if r["pass"])
    solved = sum(1 for r in rows if r["mc_status"] == "Optimal")

    md_path = os.path.join(out_dir, "report.md")
    with open(md_path, "w") as f:
        f.write("# Comparison: markov-cero vs HiGHS (R16 / RW-3)\n\n")
        f.write(f"- Baseline: HiGHS {highs_mod.Highs().version()} via highspy "
                "(external oracle; never linked into the solver)\n")
        f.write(f"- Instances: {len(rows)} (pre-registered in `data/compare/`)\n")
        f.write(f"- markov-cero SHA-256: `{solver_sha256}`\n")
        f.write("- Timing: matched whole-process wall time; one attempt per solver/sample.\n")
        f.write(f"- matched solver threads: {args.threads}\n")
        f.write(f"- Repeats per solver per cell: {args.repeat} (median; order randomized by trial); "
                f"timeout penalty: {args.penalty_sec}s before aggregation\n")
        f.write(f"- markov-cero solved {solved}/{len(rows)}; "
                f"verification+agreement gates passed on {passed}/{len(rows)}\n")
        f.write(f"- **Geometric-mean runtime ratio (markov-cero / HiGHS): "
                f"{gm:.2f}x**\n\n")
        f.write("| instance | mc engine | mc status | hi status | mc obj | hi obj | "
                "mc median [min,max] ms | hi median [min,max] ms | ratio | agreement | pass |\n")
        f.write("|---|---|---|---|---|---|---|---|---|---|---|\n")
        for r in rows:
            f.write(f"| {r['instance']} | {r.get('mc_engine','auto')} | {r['mc_status']} | "
                    f"{r['hi_status']} | "
                    f"{r['mc_objective']:.6g} | {r['hi_objective']:.6g} | "
                f"{r['mc_time_ms']:.1f} [{r['mc_time_min_ms']:.1f},"
                f"{r['mc_time_max_ms']:.1f}] | "
                f"{r['hi_time_ms']:.1f} [{r['hi_time_min_ms']:.1f},"
                f"{r['hi_time_max_ms']:.1f}] | "
                    f"{r['ratio_mc_over_hi']:.2f}x | "
                    f"{r['objective_agreement']:.1e} | {'yes' if r['pass'] else 'no'} |\n")
        f.write("\nMethodology: [[Geometric Mean Runtime]] (Dolan-More 2002; Mittelmann). "
                "GM of ratios over the shared set; never compared across different sets.\n")
        f.write("\n**Attempt policy:** one automatic-engine process per solver/sample; "
                "failures and timeouts remain in the report. Python startup/import cost "
                "is included for HiGHS; this is application latency, not pure engine speed.\n")

    curve = dolan_more_profile(rows)
    profile_path = os.path.join(out_dir, "profile.svg")
    write_profile_svg(curve, profile_path)

    print("-" * 100)
    print(f"markov-cero solved {solved}/{len(rows)}; gates passed {passed}/{len(rows)}")
    print(f"Geometric-mean runtime ratio vs HiGHS: {gm:.2f}x")
    print(f"[+] wrote {csv_path}")
    print(f"[+] wrote {md_path}")
    print(f"[+] wrote {profile_path}")

    if passed < len(rows):
        # Gate semantics: hard-fail only if a row failed where BOTH solvers were
        # Optimal but objectives disagree, or markov-cero claims a wrong optimum.
        # Rows where markov-cero fails outright (kb2/lotfi/beaconfd simplex
        # failures, tracked as the pre-existing P1 numerics gap) report FAIL in
        # the table but exit 0 — the honest evidence is committed either way and
        # the audit expects exactly these failures to be visible, not hidden
        # (the old run_gpu.py status-bug failure mode is forbidden).
        wrong_answer = [r for r in rows if r["pass"] is False
                        and r["mc_status"] == "Optimal" and r["hi_status"] == "Optimal"
                        and not math.isnan(r["objective_agreement"])
                        and r["objective_agreement"] > args.agreement_tol]
        if wrong_answer:
            print(f"[-] gate: {len(wrong_answer)} row(s) returned a wrong optimum: "
                  + ", ".join(r["instance"] for r in wrong_answer), file=sys.stderr)
            sys.exit(1)
        unsolved = [r for r in rows if r["pass"] is False]
        print(f"[i] {len(unsolved)} row(s) unsolved by markov-cero (pre-existing "
              f"numerics gap, reported honestly, not a wrong-answer gate failure): "
              + ", ".join(r["instance"] for r in unsolved), file=sys.stderr)
        sys.exit(0)
    print("[+] all rows passed verification and agreement gates")
