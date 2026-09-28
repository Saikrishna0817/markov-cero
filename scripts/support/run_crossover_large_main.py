from __future__ import annotations
from .run_crossover_large_config import (
    DEFAULT_LOGS, DEFAULT_TARGET, FIELDNAMES, SPECS, argparse, csv, os, sys, time
)
from .run_crossover_large_solve_once import generate_instance
from .run_crossover_large_solve_once import probe_cuda
from .run_crossover_large_solve_once import solve_once

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Empirical crossover benchmark: large synthetic LPs "
                    "up to 5M nonzeros")
    parser.add_argument("--solver", default="./build_gap/markov-cero-solve")
    parser.add_argument("--target", default=DEFAULT_TARGET,
                        help="directory for generated instances (keep it under "
                             "/tmp; data/ is off-limits for this study)")
    parser.add_argument("--logs", default=DEFAULT_LOGS)
    parser.add_argument("--timeout", type=float, default=300.0,
                        help="per-run wall-clock cap in seconds")
    parser.add_argument("--output", default="evidence/benchmarks/crossover_study.csv")
    parser.add_argument("--instances", nargs="*", default=[s[0] for s in SPECS])
    parser.add_argument("--append", action="store_true",
                        help="append to an existing CSV instead of replacing it")
    parser.add_argument("--include-gpu", action="store_true",
                        help="run the gpu backend even if the binary looks "
                             "CPU-only (diagnostics only)")
    args = parser.parse_args()

    if not os.path.exists(args.solver):
        print("solver not found: %s" % args.solver, file=sys.stderr)
        return 2
    os.makedirs(args.target, exist_ok=True)
    os.makedirs(args.logs, exist_ok=True)

    runs = [("cpu_pdlp", "pdlp", "cpu"), ("cpu_simplex", "primal", "cpu")]
    gpu_ok, gpu_reason = probe_cuda(args.solver)
    if args.include_gpu:
        gpu_ok, gpu_reason = True, "forced by --include-gpu"
    if gpu_ok:
        runs.append(("gpu_pdlp", "pdlp", "gpu"))
        print("gpu backend: AVAILABLE (%s)" % gpu_reason, flush=True)
    else:
        print("gpu backend: SKIPPED -- %s" % gpu_reason, flush=True)
    print("per-run wall cap: %.0fs; runs per instance: %s"
          % (args.timeout, ", ".join(r[0] for r in runs)), flush=True)

    spec_by_name = {s[0]: s for s in SPECS}
    write_header = True
    mode = "w"
    if args.append and os.path.exists(args.output):
        mode = "a"
        write_header = os.path.getsize(args.output) == 0

    study_started = time.perf_counter()
    counts: dict[str, int] = {}
    achieved = []
    with open(args.output, mode, newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=FIELDNAMES)
        if write_header:
            writer.writeheader()
        for name in args.instances:
            spec = spec_by_name.get(name)
            if spec is None:
                print("unknown instance %r; skipping" % name, file=sys.stderr)
                continue
            _, m, k, seed = spec
            info = generate_instance(name, m, k, seed, args.target)
            achieved.append((name, info["nnz"]))
            for label, engine, backend in runs:
                tag = "%s_%s" % (name, label)
                result = solve_once(args.solver, info["path"], engine, backend,
                                    args.timeout, args.logs, tag)
                row = {"instance": name, "backend": label, "nnz": info["nnz"],
                       "status": result["status"],
                       "verified": str(bool(result["verified"])),
                       "total_ms": repr(result["total_ms"])}
                writer.writerow(row)
                fh.flush()
                counts[label] = counts.get(label, 0) + 1
                print("%-12s %-11s nnz=%-8d %-14s verified=%-5s %10.1f ms"
                      % (name, label, info["nnz"], result["status"],
                         bool(result["verified"]), result["total_ms"]),
                      flush=True)

    wall = time.perf_counter() - study_started
    print("\nstudy wall time: %.1f min" % (wall / 60.0))
    print("rows written to %s: %s" % (args.output,
                                       ", ".join("%s=%d" % kv
                                                 for kv in sorted(counts.items()))))
    print("scales achieved: " +
          ", ".join("%s (%d nnz)" % (n, v) for n, v in achieved))
    if not gpu_ok:
        print("gpu rows skipped: %s" % gpu_reason)
    return 0
