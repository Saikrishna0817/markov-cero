#!/usr/bin/env python3
"""RES-01 slice D: wall-overrun and peak-RSS distributions for cooperative stops.

Blueprint step 3 / benchmark requirement: run the CLI on small and larger
inputs under tight wall-clock and memory limits, one fresh child process per
trial so `getrusage(RUSAGE_CHILDREN).ru_maxrss` stays per-trial (KiB on
Linux), and record:

  - `runtime_ms` from the result JSON versus the configured limit (overrun),
  - process wall time and per-trial peak RSS,
  - status / stop_reason / memory_charged_peak_bytes,
  - an empirical upper bound on the cooperative polling gap: the largest
    observed overrun across all deadline trials.

The benchmark measures, it does not assert a hard cap: RSS is never expected
to sit under memory_limit_bytes (see docs/contracts/resource-limits.md §4).

Usage:
  .venv/bin/python scripts/bench_resource_envelope.py \
      --cli <build>/markov-cero-solve \
      --out evidence/resource-overrun-rss-20260930.json
"""

import argparse
import json
import pathlib
import platform
import resource
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]


def one_trial(cli: str, model: str, args: list) -> dict:
    cmd = [cli, model, *args]
    start = time.monotonic()
    proc = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    wall = time.monotonic() - start
    rss_kib = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    try:
        result = json.loads(proc.stdout)
    except json.JSONDecodeError:
        result = {}
    return {
        "model": model,
        "args": args,
        "returncode": proc.returncode,
        "wall_seconds": round(wall, 4),
        "rss_kib": rss_kib,
        "status": result.get("status"),
        "stop_reason": result.get("stop_reason", ""),
        "runtime_ms": result.get("runtime_ms"),
        "memory_charged_peak_bytes": result.get("memory_charged_peak_bytes"),
        "stderr_tail": "" if proc.returncode == 0 else proc.stderr[-200:],
    }


def stats(values: list) -> dict:
    if not values:
        return {}
    ordered = sorted(values)
    return {
        "min": round(ordered[0], 6),
        "max": round(ordered[-1], 6),
        "mean": round(sum(ordered) / len(ordered), 6),
        "median": round(ordered[len(ordered) // 2], 6),
        "n": len(ordered),
    }


def summarize(case: dict, trials: list) -> dict:
    limit = case.get("limit_seconds")
    runtimes = [t["runtime_ms"] / 1000.0 for t in trials
                if t.get("runtime_ms") is not None]
    overruns = [max(0.0, r - limit) for r in runtimes] if limit else []
    peaks = [t["memory_charged_peak_bytes"] for t in trials
             if t.get("memory_charged_peak_bytes") is not None]
    return {
        "model": case["model"],
        "args": case["args"],
        "limit_seconds": limit,
        "statuses": sorted({str(t.get("status")) for t in trials}),
        "stop_reasons": sorted({str(t.get("stop_reason")) for t in trials}),
        "runtime_seconds": stats(runtimes),
        "wall_seconds": stats([t["wall_seconds"] for t in trials]),
        "overrun_seconds": stats(overruns),
        "rss_kib": stats([t["rss_kib"] for t in trials]),
        "memory_charged_peak_bytes": stats(peaks),
        "returncodes": sorted({t["returncode"] for t in trials}),
    }


# name, model, extra CLI args, trials, optional configured limit (seconds).
CASES = [
    {"name": "blend_deadline_1us", "model": "examples/blend.mps",
     "args": ["--time-limit", "0.000001"], "trials": 3, "limit_seconds": 1e-6},
    {"name": "large_deadline_250ms", "model": "data/cases/process_network_large.mps",
     "args": ["--time-limit", "0.25"], "trials": 2, "limit_seconds": 0.25},
    {"name": "large_deadline_1s", "model": "data/cases/process_network_large.mps",
     "args": ["--time-limit", "1"], "trials": 5, "limit_seconds": 1.0},
    {"name": "large_memory_64k", "model": "data/cases/process_network_large.mps",
     "args": ["--memory-limit-bytes", "65536"], "trials": 2},
    {"name": "blend_baseline", "model": "examples/blend.mps",
     "args": [], "trials": 2},
    {"name": "large_baseline", "model": "data/cases/process_network_large.mps",
     "args": [], "trials": 1},
]


def run_parent(cli: str) -> dict:
    trials_by_case = []
    for case in CASES:
        trials = []
        for _ in range(case["trials"]):
            child_cmd = [sys.executable, str(pathlib.Path(__file__).resolve()),
                         "--child", "--cli", cli, "--model", case["model"],
                         "--args-json", json.dumps(case["args"])]
            proc = subprocess.run(child_cmd, capture_output=True, text=True)
            if proc.returncode != 0:
                raise SystemExit(f"trial wrapper failed: {proc.stderr[-400:]}")
            trials.append(json.loads(proc.stdout))
        trials_by_case.append((case, trials))
    return trials_by_case


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, help="markov-cero-solve binary")
    parser.add_argument("--model", help="child mode: model path")
    parser.add_argument("--args-json", default="[]",
                        help="child mode: CLI args as a JSON array")
    parser.add_argument("--child", action="store_true")
    parser.add_argument("--out", help="evidence JSON destination")
    args = parser.parse_args()

    if args.child:
        trial_args = json.loads(args.args_json)
        print(json.dumps(one_trial(args.cli, args.model, trial_args)))
        return 0

    cli = str(pathlib.Path(args.cli).resolve())
    started = time.strftime("%Y-%m-%dT%H:%M:%S%z")
    cases = run_parent(cli)

    distributions, raw = {}, []
    max_overrun, memory_runs = 0.0, []
    for case, trials in cases:
        distributions[case["name"]] = summarize(case, trials)
        raw.extend(trials)
        overrun = distributions[case["name"]].get("overrun_seconds", {})
        if case.get("limit_seconds") and overrun:
            max_overrun = max(max_overrun, overrun["max"])
        if any("--memory-limit-bytes" in t["args"] for t in trials):
            memory_runs.extend(trials)

    rss_bytes = None
    if memory_runs:
        rss_bytes = max(t["rss_kib"] for t in memory_runs) * 1024
    evidence = {
        "date": time.strftime("%Y-%m-%d"),
        "run_started": started,
        "purpose": ("RES-01 slice D: measured wall-overrun / peak-RSS envelope "
                    "and empirical cooperative polling bound (blueprint steps 3 "
                    "and benchmark requirement)."),
        "commands": {
            "benchmark": (f".venv/bin/python scripts/bench_resource_envelope.py "
                          f"--cli {args.cli} --out <out.json>"),
            "contract": "docs/contracts/resource-limits.md",
        },
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "cli": cli,
        },
        "distributions": distributions,
        "trials": raw,
        "polling_bound": {
            "max_observed_deadline_overrun_seconds": round(max_overrun, 6),
            "interpretation": ("Largest measured gap between configured "
                               "deadline and recorded stop across deadline "
                               "trials: an empirical upper bound on one "
                               "cooperative polling interval on these inputs, "
                               "not a guaranteed bound (section 4)."),
        },
        "rss_vs_budget": {
            "memory_limit_bytes": 65536,
            "max_rss_bytes_under_that_cap": rss_bytes,
            "interpretation": ("memory_limit_bytes meters instrumented "
                               "charges only; measured process RSS stays far "
                               "above the cap, as the contract states."),
        },
        "limitations": [
            "Cooperative stops only: a single indivisible kernel between polls "
            "extends the observed overrun; no preemption is claimed.",
            "ru_maxrss is per-child on Linux (KiB); this machine's RSS is not "
            "portable to other hosts or input distributions.",
            "runtime_ms spans API entry through engine completion, before JSON "
            "serialization; process wall_seconds additionally includes startup.",
            "Small trial counts characterize this run, not a distributional "
            "guarantee.",
        ],
    }
    out_path = pathlib.Path(args.out) if args.out else None
    if out_path:
        if not out_path.is_absolute():
            out_path = ROOT / out_path
        out_path.write_text(json.dumps(evidence, indent=1) + "\n")
        print(f"wrote {out_path}")
    print(json.dumps({name: s["overrun_seconds"] for name, s in
                      distributions.items() if s.get("overrun_seconds")}, indent=1))
    print(f"max deadline overrun: {max_overrun:.6f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
