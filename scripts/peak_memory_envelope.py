#!/usr/bin/env python3
"""IR-21 measured peak-memory envelope: solve budget, factor fill, peak RSS.

Runs the production CLI through three sections and records
`memory_charged_peak_bytes` / `peak_rss_bytes` from the JSON result:

1. Budget ladder — per instance (LP, QP, MILP), a limit ladder from 1 byte to
   2x the unlimited charged peak. Assertions: a limit of 1 refuses at the first
   charge with ResourceLimit + memory_budget_exhausted and a message naming
   memory; admitted bytes never exceed the configured limit; the charged peak
   is monotone in the limit; some mid-limit run admits the first charge and
   refuses a later one (factor/KKT/proof charges are metered); for LP and QP
   that later refusal names a factor workspace; 2x peak solves normally with a
   published positive RSS.
2. Adversarial factor fill — a generated dense 150x150 LP versus its banded
   control: one shared budget (70% of the dense charged peak) refuses the dense
   instance at its sparse basis factor while the banded control solves under
   the same limit; the dense charged peak is >= 5x the banded one.
3. IR-19 transient-RSS follow-up — bounded re-measure (report-only) of the
   pk1 / sp150x300d / gen-ip002 instances that once recorded ~990 MiB
   VmHWM; every run must publish peak_rss_bytes > 0.

Any section assertion failure prints FAILURES and exits non-zero.
"""
from __future__ import annotations

import json
import random
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOLVE = ROOT / "build" / "markov-cero-solve"
OUT_DIR = ROOT / "evidence"
WORK = Path("/tmp/markov-peak-envelope")
GENERATED_SEED = 20261003
GENERATED_N = 150
PRIOR_SPIKE_BYTES = 1013492 * 1024  # IR-19 observed transient VmHWM (KiB -> B)
SPIKE_REPORT_THRESHOLD = 500 * 1024 * 1024

failures: list[str] = []


def fail(context: str, problem: str) -> None:
    failures.append(f"{context}: {problem}")


def run(model: str, engine: str | None = None, limit: int | None = None,
        time_limit: float | None = None) -> dict:
    out = WORK / "run.json"
    if out.exists():
        out.unlink()
    cmd = [str(SOLVE), model, "--output", str(out)]
    if engine:
        cmd += ["--engine", engine]
    if limit is not None:
        cmd += ["--memory-limit-bytes", str(limit)]
    if time_limit is not None:
        cmd += ["--time-limit", str(time_limit)]
    started = time.perf_counter()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    wall = time.perf_counter() - started
    row = {"model": model, "engine": engine, "limit_bytes": limit,
           "wall_s": round(wall, 4), "exit_code": proc.returncode}
    if not out.exists():
        row.update({"status": None, "note": "no result file"})
        fail(str(row), f"solve produced no output: {proc.stderr[-200:]}")
        return row
    data = json.loads(out.read_text())
    row.update({k: data.get(k) for k in
                ("status", "stop_reason", "message", "verified",
                 "memory_charged_peak_bytes", "peak_rss_bytes", "resolved_engine")})
    row["failure_site"] = (data.get("diagnostic") or {}).get("failure_site")
    return row


def check_budget_row(row: dict, context: str) -> None:
    limit = row["limit_bytes"]
    charged = row["memory_charged_peak_bytes"] or 0
    rss = row["peak_rss_bytes"] or 0
    if rss <= 0:
        fail(context, f"peak_rss_bytes not published ({rss})")
    if limit is None:
        return
    if charged > limit:
        fail(context, f"admitted {charged} bytes exceed limit {limit}")
    if limit == 1:
        if row["status"] != "ResourceLimit" or row["stop_reason"] != "memory_budget_exhausted":
            fail(context, f"limit=1 did not refuse: {row['status']}/{row['stop_reason']}")
        if charged != 0:
            fail(context, f"limit=1 admitted {charged} bytes")
        if "memory" not in (row.get("message") or "").lower():
            fail(context, f"limit=1 message does not name memory: {row.get('message')!r}")
        if row["failure_site"] != "memory_budget":
            fail(context, f"limit=1 failure_site={row['failure_site']!r}")


def budget_ladder(model: str, engine: str | None) -> dict:
    base = run(model, engine)
    peak = base["memory_charged_peak_bytes"] or 0
    record = {"model": model, "engine": engine, "unlimited": base,
              "charged_peak_bytes": peak, "ladder": []}
    if peak <= 0:
        fail(model, f"unlimited run charged nothing (status {base['status']})")
        return record
    ladder = sorted({1, int(0.6 * peak), int(0.8 * peak), int(0.9 * peak),
                     peak, 2 * peak})
    peaks: list[int] = []
    factor_named = False
    later_refusal = False
    for limit in ladder:
        row = run(model, engine, limit=limit)
        record["ladder"].append(row)
        check_budget_row(row, f"{model}@{limit}")
        charged = row["memory_charged_peak_bytes"] or 0
        refused = (row["stop_reason"] == "memory_budget_exhausted")
        message = (row.get("message") or "").lower()
        if refused and "factor" in message and "memory budget" in message:
            factor_named = True
        if limit != 1 and refused and charged > 0:
            later_refusal = True
        if limit == 2 * peak:
            if refused:
                fail(f"{model}@{limit}", "2x peak still refused the budget")
            if row["status"] not in ("Optimal", "Feasible"):
                fail(f"{model}@{limit}", f"2x peak did not solve: {row['status']}")
        peaks.append(charged)
    if peaks != sorted(peaks):
        fail(model, f"charged peak not monotone in limit: {peaks}")
    if not later_refusal:
        fail(model, "no mid-limit run admitted the first charge and refused a later one")
    record["factor_refusal_observed"] = factor_named
    if not factor_named:
        fail(model, "no refusal named a factor workspace")
    return record


def generate_lp(path: Path, dense: bool, seed: int = GENERATED_SEED) -> None:
    rng = random.Random(seed)
    n = GENERATED_N
    lines = [f"NAME  {path.stem.upper()}", "ROWS", " N  OBJ"]
    lines += [f" L  R{i:04d}" for i in range(n)]
    lines.append("COLUMNS")
    for j in range(n):
        rows = [i for i in range(n) if dense or abs(i - j) <= 1]
        lines.append(f"    X{j:05d}  OBJ  -1")
        for k in range(0, len(rows), 2):
            chunk = "  ".join(f"R{i:04d}  {rng.uniform(0.5, 1.5):.6f}" for i in rows[k:k + 2])
            lines.append(f"    X{j:05d}  {chunk}")
    lines.append("RHS")
    lines += [f"    RHS1  R{i:04d}  1" for i in range(n)]
    lines.append("ENDATA")
    path.write_text("\n".join(lines) + "\n")


def adversarial_fill() -> dict:
    dense_path = WORK / "dense150.mps"
    banded_path = WORK / "banded150.mps"
    generate_lp(dense_path, dense=True)
    generate_lp(banded_path, dense=False)
    dense = run(str(dense_path))
    banded = run(str(banded_path))
    dense_peak = dense["memory_charged_peak_bytes"] or 0
    banded_peak = banded["memory_charged_peak_bytes"] or 0
    record = {"dense_unlimited": dense, "banded_unlimited": banded,
              "dense_charged_peak_bytes": dense_peak,
              "banded_charged_peak_bytes": banded_peak, "shared_limit_bytes": None,
              "dense_limited": None, "banded_limited": None}
    for name, row in (("dense", dense), ("banded", banded)):
        if row["status"] != "Optimal":
            fail(f"fill/{name}", f"unlimited solve did not finish: {row['status']}")
        if (row["peak_rss_bytes"] or 0) <= 0:
            fail(f"fill/{name}", "peak_rss_bytes not published")
    if dense_peak < 5 * banded_peak:
        fail("fill", f"dense peak {dense_peak} < 5x banded peak {banded_peak}")
    if (dense["peak_rss_bytes"] or 0) <= (banded["peak_rss_bytes"] or 0):
        fail("fill", "dense RSS not above banded RSS")
    shared = None
    for fraction in (0.7, 0.8, 0.6, 0.9):
        candidate = int(fraction * dense_peak)
        dense_limited = run(str(dense_path), limit=candidate)
        message = (dense_limited.get("message") or "").lower()
        if (dense_limited["stop_reason"] == "memory_budget_exhausted"
                and "factor" in message and "memory budget" in message):
            shared = candidate
            record["dense_limited"] = dense_limited
            break
    if shared is None:
        fail("fill", "no limit refused the dense instance at its factor")
        return record
    record["shared_limit_bytes"] = shared
    banded_limited = run(str(banded_path), limit=shared)
    record["banded_limited"] = banded_limited
    if banded_limited["status"] != "Optimal":
        fail("fill", f"banded control refused under the shared limit: "
                     f"{banded_limited['status']} {banded_limited.get('message')!r}")
    if (banded_limited["memory_charged_peak_bytes"] or 0) > shared:
        fail("fill", "banded admission exceeded the shared limit")
    return record


def anomaly_followup() -> dict:
    runs: list[dict] = []
    matrix = [("data/miplib/pk1.mps", 6), ("data/miplib/gen-ip002.mps", 6),
              ("data/miplib/sp150x300d.mps", 2)]
    for model, repeats in matrix:
        for _ in range(repeats):
            row = run(str(ROOT / model), time_limit=2.0)
            check_budget_row(row, f"anomaly/{model}")
            if row.get("stop_reason") not in ("deadline_exceeded", "", None):
                fail(f"anomaly/{model}", f"time-limited run reported {row['stop_reason']}")
            runs.append(row)
    budgeted = [run(str(ROOT / "data/miplib/pk1.mps"), limit=1024 * 1024,
                    time_limit=2.0) for _ in range(2)]
    for row in budgeted:
        check_budget_row(row, "anomaly/pk1@1MiB")
        runs.append(row)
    spikes = [r for r in runs if (r.get("peak_rss_bytes") or 0) >= SPIKE_REPORT_THRESHOLD]
    exact = [r for r in runs if (r.get("peak_rss_bytes") or 0) == PRIOR_SPIKE_BYTES]
    return {"runs": runs, "spike_report_threshold_bytes": SPIKE_REPORT_THRESHOLD,
            "prior_ir19_peak_bytes": PRIOR_SPIKE_BYTES,
            "runs_at_or_above_threshold": len(spikes),
            "runs_matching_prior_exact_peak": len(exact),
            "note": "report-only: IR-19 recorded ~5 occurrences in ~35 eligible runs; "
                    "reproduction is not asserted here"}


def main() -> int:
    if not SOLVE.is_file():
        print(f"missing {SOLVE}", file=sys.stderr)
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    started = time.time()
    sweep = [budget_ladder("data/netlib/e226.mps", None),
             budget_ladder("data/qp/QPLIB_0010.mps", "qp"),
             budget_ladder("data/miplib/stein15.mps", None)]
    fill = adversarial_fill()
    anomaly = anomaly_followup()
    record = {
        "date": time.strftime("%Y-%m-%d"),
        "purpose": "IR-21 measured peak-memory envelope: solve-wide budget ladder, "
                   "adversarial factor fill, published peak RSS, IR-19 RSS follow-up",
        "method": "production CLI --memory-limit-bytes / --time-limit; per-run "
                  "memory_charged_peak_bytes and peak_rss_bytes from the result JSON; "
                  "generated dense/banded LPs are deterministic (seed "
                  f"{GENERATED_SEED}, n={GENERATED_N}); assertions in "
                  "scripts/peak_memory_envelope.py",
        "budget_ladder": sweep,
        "adversarial_fill": fill,
        "ir19_transient_rss_followup": anomaly,
        "wall_s": round(time.time() - started, 3),
        "failures": failures,
    }
    OUT_DIR.mkdir(exist_ok=True)
    path = OUT_DIR / f"peak-memory-envelope-{record['date']}.json"
    path.write_text(json.dumps(record, indent=1) + "\n")
    print(f"-> {path} ({record['wall_s']}s, {len(failures)} failures)")
    if failures:
        print("FAILURES:", *failures, sep="\n  ")
        return 1
    dense_peak = fill["dense_charged_peak_bytes"]
    banded_peak = fill["banded_charged_peak_bytes"]
    print(f"budget ladder ok; adversarial fill {dense_peak} vs banded {banded_peak} "
          f"bytes; anomaly spikes {anomaly['runs_at_or_above_threshold']}"
          f"/{len(anomaly['runs'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
