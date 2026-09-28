from __future__ import annotations
from .run_full_compare_config import (
    CURATED, OUT, REPO, SCRIPTS, SOLVER_ORDER, ThreadPoolExecutor, argparse, hashlib, os, shutil, subprocess, sys, time
)
from .run_full_compare_annotate import aggregates
from .run_full_compare_annotate import annotate
from .run_full_compare_build_report import build_report
from .run_full_compare_run_highs import ensure_runtime_env
from .run_full_compare_annotate import load_reference_objectives
from .run_full_compare_run_highs import mps_path
from .run_full_compare_write_plan_comparison_exports import parse_mittelmann_reference
from .run_full_compare_annotate import probe_availability
from .run_full_compare_run_glpk import solve_task
from .run_full_compare_write_plan_comparison_exports import write_mittelmann_reference
from .run_full_compare_write_plan_comparison_exports import write_plan_comparison_exports
from .run_full_compare_annotate import write_results_csv

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--solver",
                    default=os.path.join(REPO, "build_gap", "markov-cero-solve"))
    ap.add_argument("--timeout", type=float, default=60.0,
                    help="per-instance per-solver time cap in seconds")
    ap.add_argument("--workers", type=int, default=1,
                    help="concurrent diagnostic runs; use 1 for timing comparisons")
    ap.add_argument("--threads", type=int, default=1,
                    help="matched solver threads per run (default: 1)")
    ap.add_argument("--instances", nargs="*", default=None,
                    help="optional instance names to run (default: curated set)")
    ap.add_argument("--suites", nargs="*", default=None,
                    choices=sorted({suite for suite, _, _ in CURATED}),
                    help="optional suites to include (default: all curated suites)")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--skip-profile", action="store_true")
    ap.add_argument("--drop-legacy-report", action="store_true",
                    help="do not quote the previous report as an appendix")
    args = ap.parse_args()

    ensure_runtime_env()
    os.makedirs(args.out, exist_ok=True)

    instances = []
    for suite, cls, name in CURATED:
        if args.instances is not None and name not in args.instances:
            continue
        if args.suites is not None and suite not in args.suites:
            continue
        path = mps_path(suite, name)
        if os.path.isfile(path):
            instances.append((suite, cls, name))
        else:
            print(f"[!] missing instance {path}, skipped", file=sys.stderr)
    if not instances:
        print("[-] no curated instances found", file=sys.stderr)
        return 2

    probes = probe_availability(args.solver)
    args.solver_sha256 = ""
    if os.path.isfile(args.solver):
        with open(args.solver, "rb") as binary_file:
            args.solver_sha256 = hashlib.file_digest(binary_file, "sha256").hexdigest()
    print("[+] availability: " + ", ".join(
        f"{name}={'yes' if probes[name]['available'] else 'no'}"
        for name in SOLVER_ORDER))

    runnable = [s for s in SOLVER_ORDER if probes.get(s, {}).get("available")]
    if not runnable:
        print("[-] no solver available", file=sys.stderr)
        return 2

    rows = []
    started = time.perf_counter()
    # Each task spends its time in a child solver process or a native solver
    # API that releases the GIL. Threads avoid multiprocessing's forkserver,
    # which is unavailable in restricted/containerized environments.
    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
        for suite, cls, name in instances:
            if args.solver_sha256:
                with open(args.solver, "rb") as binary_file:
                    current_sha256 = hashlib.file_digest(binary_file, "sha256").hexdigest()
                if current_sha256 != args.solver_sha256:
                    print("[-] markov-cero binary changed during comparison run",
                          file=sys.stderr)
                    return 2
            path = mps_path(suite, name)
            futures = [pool.submit(solve_task, {
                "solver": solver, "instance": name, "mps": path,
                "timeout": args.timeout, "threads": args.threads,
                "binary": args.solver})
                for solver in runnable]
            for future in futures:
                try:
                    rows.append(future.result(timeout=args.timeout + 45))
                except Exception as exc:
                    rows.append({"instance": name, "solver": "unknown",
                                 "status": f"Error({type(exc).__name__})",
                                 "objective": "", "runtime_ms": 0.0,
                                 "certified": False, "detail": str(exc)})
            done = len([r for r in rows if r["instance"] == name])
            print(f"  [{done:3d}/{len(instances) * len(runnable)}] {suite}/{name}")
    for row in rows:
        row["markov_cero_sha256"] = args.solver_sha256
    wall_seconds = time.perf_counter() - started

    order = {name: i for i, (_, _, name) in enumerate(instances)}
    solver_order = {s: i for i, s in enumerate(SOLVER_ORDER)}
    rows.sort(key=lambda r: (order.get(r["instance"], 999),
                             solver_order.get(r["solver"], 999)))

    versions = {r["solver"]: r.get("version") or probes.get(
        r["solver"], {}).get("version", "unknown")
        for r in rows if r.get("version")}
    if "markov-cero" in versions:
        probes["markov-cero"]["version"] = versions["markov-cero"]

    refs = load_reference_objectives(instances)
    _, disagreements = annotate(rows, refs, instances)
    aggregates_rows = aggregates(rows, probes)

    csv_path = os.path.join(args.out, "full_compare_results.csv")
    write_results_csv(csv_path, rows)
    print(f"[+] {len(rows)} rows -> {csv_path}")
    plan_exports = write_plan_comparison_exports(args.out, rows, instances, refs)
    if os.path.abspath(args.out) == os.path.abspath(OUT):
        # Preserve the pre-existing W9 tables before updating their filenames
        # to the plan's all-solver schema.
        for filename in ("netlib_comparison.csv", "miplib_comparison.csv"):
            path = os.path.join(args.out, filename)
            legacy = os.path.join(args.out, filename[:-4] + "_legacy.csv")
            if os.path.isfile(path) and not os.path.exists(legacy):
                shutil.copy2(path, legacy)
    print("[+] plan-facing suite tables -> " + ", ".join(
        os.path.join(args.out, name) for name in plan_exports))

    reference_path = os.path.join(args.out, "mittelmann_reference.md")
    with open(reference_path, "w") as f:
        f.write(write_mittelmann_reference(instances))
    print(f"[+] reference -> {reference_path}")
    reference_entries = parse_mittelmann_reference(reference_path)

    svg_path = os.path.join(args.out, "dolan_more_runtime_profile.svg")
    profile_data_path = os.path.join(args.out, "dolan_more_profile_data.csv")
    profile_ok = False
    if not args.skip_profile:
        profile = subprocess.run(
            [sys.executable, os.path.join(SCRIPTS, "dolan_more_profile.py"),
             "--input", csv_path, "--svg", svg_path, "--csv", profile_data_path],
            capture_output=True, text=True, cwd=REPO)
        sys.stdout.write(profile.stdout)
        if profile.returncode != 0:
            sys.stderr.write(profile.stderr)
            print(f"[-] profile generation failed (rc={profile.returncode})",
                  file=sys.stderr)
        else:
            profile_ok = True

    markov_optimal = {r["instance"]: r["objective"] for r in rows
                      if r["solver"] == "markov-cero" and r["status"] == "Optimal"}
    report = build_report(args, probes, rows, instances, refs, markov_optimal,
                          disagreements, aggregates_rows, reference_entries,
                          wall_seconds,
                          svg_path if profile_ok else "(not generated)",
                          csv_path, profile_data_path, versions, {})
    report_path = os.path.join(args.out, "full_comparison_report.md")
    with open(report_path, "w") as f:
        f.write(report)
    print(f"[+] report -> {report_path}")

    print(f"[+] wall clock {wall_seconds / 60.0:.1f} min; "
          f"optimal {sum(1 for r in rows if r['status'] == 'Optimal')}/{len(rows)}; "
          f"verified {sum(1 for r in rows if r.get('verified'))}/{len(rows)}; "
          f"disagreements {len(disagreements)}")
    return 0
