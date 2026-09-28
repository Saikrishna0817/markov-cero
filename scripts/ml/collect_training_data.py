#!/usr/bin/env python3
"""W2 / D-05: training-data collection for ML branching.

Runs the markov-cero solver on MIPLIB 2017 easy instances with strong
branching enabled and records, for every B&B node where strong branching was
evaluated, the per-candidate features and the strong-branching scores.

The C++ solver emits these records when MARKOV_CERO_ENABLE_ML=ON and the env
var MARKOV_CERO_SB_LOG is set: the log is a compact MCONLOG3 binary stream
with variable features, active rows, variable-row edges, and SB scores.

FEATURE ORDER (Feature 27 — C++ is ground truth, this script never reorders):
    Variable features are written and consumed by the graph scorer in order:
        0 fractionality | 1 objective_coefficient | 2 pseudocost_down_ratio |
        3 pseudocost_up_ratio | 4 bound_width | 5 column_density
    (struct fields: include/markov_cero/milp/branch_selector.hpp:29-34).
    Alignment preconditions that keep features[k] zipped with sb_scores[k]:
      * StrongBranchingOptions.max_candidates == 0 (default), so the
        fractionality sort/truncation at src/milp/strong_branching.cpp:38-49
        never fires and SB evaluates candidates in ascending column order;
      * the solver's integrality tolerance (1e-6,
        include/markov_cero/milp/milp_solver.hpp:20) equals the tolerance
        used by extract_features_static (onnx_scorer.cpp:135), so both sides
        select the same fractional set (find_fractional_variables iterates
        columns ascending: src/milp/branch_selector.cpp:49-63).
    Every produced log is domain-audited after the run
    (feature_spec.audit_feature_matrix): a column-order bug would break
    fractionality in [0,0.5] or the pseudocost ratio pair summing to 1.

SPLIT METADATA (Feature 26): each provenance record carries `started_at` /
`started_epoch` so downstream partitioning can do a chronological split;
instance ids (= output subdirectory names) are what the 70/15/15 instance
split operates on.

Output is gitignored (D-19): data/ml_training/<instance>/*.bin.
Provenance JSON is committed next to the run manifest.

Usage:
    python3 scripts/ml/collect_training_data.py \
        --instances data/miplib/*.mps --out data/ml_training \
        --solver build_w5/markov-cero-solve --timeout 300
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

MAGIC = b"MCONLOG3"

sys.path.insert(0, str(Path(__file__).resolve().parent))
import feature_spec  # noqa: E402
import ml_data  # noqa: E402


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def collect_one(solver: str, instance: Path, out_dir: Path, timeout: int) -> dict:
    """Run one solve with SB logging enabled; return a provenance record."""
    out_dir.mkdir(parents=True, exist_ok=True)
    log_path = out_dir / f"{instance.stem}.sb.bin"
    solve_path = out_dir / f"{instance.stem}.json"
    if log_path.exists():
        # The solver opens the log in APPEND mode (milp_solver.cpp:118-122),
        # so a rerun would duplicate this instance's records and corrupt the
        # dataset counts the 70/15/15 split reports.  Start clean.
        log_path.unlink()
    env = dict(os.environ)
    env["MARKOV_CERO_SB_LOG"] = str(log_path)

    started = time.time()
    started_at = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    # Let the solver stop ITSELF just before our subprocess timeout: a clean
    # exit flushes the buffered std::ofstream SB log, whereas SIGKILL from
    # subprocess.run(timeout=...) discards every buffered record and the
    # instance would appear to have produced no training data at all.
    solver_time_limit = max(1, timeout - 2)
    try:
        proc = subprocess.run(
            [solver, str(instance), "--branching", "strong_branching",
             "--time-limit", str(solver_time_limit),
             "--output", str(solve_path)],
            env=env, capture_output=True, timeout=timeout,
        )
        elapsed = time.time() - started
        status = "ok" if proc.returncode == 0 else f"exit_{proc.returncode}"
    except subprocess.TimeoutExpired:
        elapsed = time.time() - started
        status = "timeout"

    record = {
        "instance": instance.name,
        "sha256": file_sha256(instance),
        "status": status,
        "seconds": round(elapsed, 2),
        "log_bytes": log_path.stat().st_size if log_path.exists() else 0,
        "solver": solver,
        "started_at": started_at,
        "started_epoch": started,
    }
    if solve_path.exists():
        try:
            solve_record = json.loads(solve_path.read_text())
            record["solver_status"] = solve_record.get("status")
            record["verified"] = solve_record.get("verified")
            record["solver_message"] = solve_record.get("message")
            record["rows"] = solve_record.get("rows")
            record["columns"] = solve_record.get("cols")
            record["nonzeros"] = solve_record.get("nonzeros")
        except (OSError, json.JSONDecodeError):
            record["solver_json_error"] = "unreadable or incomplete solver JSON"
    if log_path.exists() and log_path.stat().st_size == 0:
        record["note"] = "empty SB log (no strong-branching nodes or ML build off)"
    return record


def audit_logs(out_root: Path) -> dict:
    """Domain-audit every produced log against the canonical feature order."""
    dataset = ml_data.load_dataset(out_root)
    feats = [X for e in dataset["instances"].values() for X in e["X"]]
    if not feats:
        return {"ok": False, "rows": 0, "violations": {"no_records": 1}}
    return feature_spec.audit_feature_matrix(np.vstack(feats))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--instances", nargs="+", required=True)
    ap.add_argument("--out", default="data/ml_training")
    ap.add_argument("--solver", default="build_w5/markov-cero-solve")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--skip-feature-audit", action="store_true")
    args = ap.parse_args()

    if not Path(args.solver).exists():
        print(f"solver not found: {args.solver}", file=sys.stderr)
        return 2

    out_root = Path(args.out)
    out_root.mkdir(parents=True, exist_ok=True)
    manifest_path = out_root / "provenance.json"
    prior_records = {}
    if manifest_path.exists():
        try:
            previous = json.loads(manifest_path.read_text())
            prior_records = {str(r.get("instance")): r
                             for r in previous.get("records", [])
                             if r.get("instance")}
        except (OSError, json.JSONDecodeError):
            prior_records = {}
    records = []
    for inst in args.instances:
        path = Path(inst)
        if not path.exists():
            print(f"skip (missing): {inst}", file=sys.stderr)
            continue
        rec = collect_one(args.solver, path, out_root / path.stem, args.timeout)
        records.append(rec)
        prior_records[rec["instance"]] = rec
        print(f"{rec['instance']}: {rec['status']} in {rec['seconds']}s, "
              f"{rec['log_bytes']} bytes of SB records")

    audit = audit_logs(out_root)
    print(f"feature-domain audit: ok={audit['ok']} rows={audit.get('rows', 0)} "
          f"violations={audit.get('violations', {})}")
    if not audit["ok"] and not args.skip_feature_audit:
        print("collected data violates the canonical feature order/domains "
              "(feature 27); refusing to write a provenance manifest that "
              "would bless a mis-ordered dataset (use --skip-feature-audit "
              "to record anyway)", file=sys.stderr)
        return 3

    manifest = {
        "magic": MAGIC.decode(),
        "generator": "scripts/ml/collect_training_data.py",
        "decision": "D-05 (self-collected strong-branching log, sovereignty R10)",
        "feature_order": list(feature_spec.FEATURE_ORDER),
        "feature_order_citations": feature_spec.FEATURE_CITATIONS,
        "feature_order_audit": audit,
        "records": sorted(prior_records.values(), key=lambda r: r["instance"]),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2))
    print(f"provenance: {manifest_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
