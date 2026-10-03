#!/usr/bin/env python3
"""IR-20 measured deadline gate: end-to-end wall-clock overrun per engine.

Runs the production CLI with the solve-wide deadline (--time-limit maps to
total_time_limit_seconds, apps/markov_cero_solve.cpp) under three profiles
per engine -- already-expired (1e-6 s), tiny (0.01 s or 0.05 s) and short
(0.5 s) -- and records whole-process wall time against the requested limit.
The harness measures what a caller experiences: parse, dispatch, solver
work, cooperative stop, and result finalization all included.

Assertions (all must hold or the script exits non-zero):
  * every run whose wall time exceeded its deadline reports
    stop_reason "deadline_exceeded" (status resource_limit) or completed
    with a terminal status before the deadline -- never a proven
    "Optimal"/"Feasible" result silently past its deadline;
  * already-expired runs stop at the first stage poll (no engine claim);
  * wall-clock overrun stays inside the envelope cap declared below.
"""
from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOLVE = ROOT / "build" / "markov-cero-solve"
OUT_DIR = ROOT / "evidence"
ENVELOPE_CAP_SECONDS = 3.0  # measured envelope; see evidence/deadline-envelope-*.json

# (engine, instance, already_expired, tiny, short)
MATRIX = [
    ("primal", "data/netlib/e226.mps", 1e-6, 0.01, 0.05),
    ("dual", "data/netlib/e226.mps", 1e-6, 0.01, 0.05),
    ("ipm", "data/netlib/e226.mps", 1e-6, 0.01, 0.05),
    ("pdlp", "data/netlib/e226.mps", 1e-6, 0.01, 0.05),
    ("qp", "data/qp/QPLIB_0010.mps", 1e-6, 0.01, 0.05),
    ("miqp", "data/miqp/miqp_large_n40.mps", 1e-6, 0.05, 0.5),
    ("milp", "data/miplib/stein15.mps", 1e-6, 0.05, 0.5),
    ("parallel", "data/miplib/stein15.mps", 1e-6, 0.05, 0.5),
    ("milp", "data/miplib/enlight_hard.mps", 1e-6, 1.0, 5.0),
    ("parallel", "data/miplib/enlight_hard.mps", 1e-6, 1.0, 5.0),
]

# Expiry-landing sweep: fine-grained limits that scatter the expiry point
# across parse, canonicalization, presolve, root LP, cut/factorization units
# and early search instead of three coarse profiles.
SWEEP = [
    ("primal", "data/netlib/e226.mps", [0.003, 0.008, 0.02, 0.05, 0.1]),
    ("milp", "data/miplib/stein15.mps", [0.003, 0.008, 0.02, 0.1, 0.3, 1.5]),
]


def run_one(engine: str, model: str, limit: float) -> dict:
    out = Path("/tmp/markov-deadline-envelope.json")
    if out.exists():
        out.unlink()
    cmd = [str(SOLVE), model, "--engine", engine, "--time-limit", str(limit),
           "--output", str(out)]
    started = time.perf_counter()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    wall = time.perf_counter() - started
    row = {"engine": engine, "model": model, "limit_s": limit,
           "wall_s": round(wall, 6), "overrun_s": round(wall - limit, 6),
           "exit_code": proc.returncode}
    if out.exists():
        data = json.loads(out.read_text())
        row.update({k: data.get(k) for k in
                    ("status", "stop_reason", "verified", "assurance",
                     "nodes_explored", "runtime_ms")})
    else:
        row.update({"status": None, "stop_reason": None, "note": "no result file"})
    return row


def check(row: dict) -> list[str]:
    problems = []
    status, reason = row.get("status"), row.get("stop_reason") or ""
    if row["overrun_s"] > ENVELOPE_CAP_SECONDS:
        problems.append(f"overrun {row['overrun_s']}s exceeds envelope cap")
    if reason == "deadline_exceeded" and status != "ResourceLimit":
        problems.append(f"deadline stop reported status {status}")
    if reason != "deadline_exceeded" and row["wall_s"] > row["limit_s"]:
        problems.append(f"ran past deadline without a deadline stop "
                        f"(status={status}, reason={reason!r})")
    if status in ("Optimal", "Feasible") and row["wall_s"] > row["limit_s"]:
        problems.append(f"{status} claimed after wall {row['wall_s']}s > limit")
    if row["profile"] == "already_expired" and reason == "" and \
            status not in ("Optimal", "Infeasible", "Unbounded"):
        problems.append(f"already-expired run did not stop (status={status})")
    return problems


def main() -> int:
    if not SOLVE.is_file():
        print(f"missing {SOLVE}", file=sys.stderr)
        return 2
    rows, failures = [], []
    work = []
    for engine, model, expired, tiny, short in MATRIX:
        work += [(engine, model, label, limit)
                 for label, limit in (("already_expired", expired), ("tiny", tiny),
                                      ("short", short))]
    for engine, model, limits in SWEEP:
        work += [(engine, model, "sweep", limit) for limit in limits]
    for engine, model, label, limit in work:
        row = run_one(engine, model, limit)
        row["profile"] = label
        for problem in check(row):
            failures.append(f"{engine} {model} {limit}: {problem}")
        rows.append(row)
        print(f"{engine:9} {label:16} limit={limit:<6} wall={row['wall_s']:.4f}s "
              f"overrun={row['overrun_s']:+.4f}s {row.get('status')}/{reason(row)}")
    per_engine = {}
    for row in rows:
        key = row["engine"]
        per_engine[key] = max(per_engine.get(key, 0.0), row["overrun_s"])
    record = {
        "date": time.strftime("%Y-%m-%d"),
        "purpose": "IR-20 measured deadline gate: end-to-end overrun envelope per engine",
        "method": "production CLI --time-limit (= SolveOptions::total_time_limit_seconds) "
                  "with whole-process wall measurement; profiles already_expired/tiny/short "
                  "per engine plus an expiry-landing sweep across preprocessing/root/search "
                  "units; assertion set in scripts/deadline_envelope.py",
        "binary_sha256_note": "see evidence/qualification-demo-transcript-2026-10-03.txt "
                              "for the release-revision binary hashes",
        "envelope_cap_seconds": ENVELOPE_CAP_SECONDS,
        "rows": rows,
        "max_overrun_by_engine_s": {k: round(v, 6) for k, v in per_engine.items()},
        "max_overrun_s": round(max(per_engine.values()), 6),
        "failures": failures,
        "assertions": "no post-deadline proven status; deadline stops map to "
                      "ResourceLimit+deadline_exceeded; wall overrun within cap",
    }
    OUT_DIR.mkdir(exist_ok=True)
    path = OUT_DIR / f"deadline-envelope-{record['date']}.json"
    path.write_text(json.dumps(record, indent=1) + "\n")
    print(f"-> {path}")
    if failures:
        print("FAILURES:", *failures, sep="\n  ")
        return 1
    print(f"all {len(rows)} runs inside envelope; max overrun "
          f"{record['max_overrun_s']}s")
    return 0


def reason(row: dict) -> str:
    return row.get("stop_reason") or "-"


if __name__ == "__main__":
    raise SystemExit(main())
