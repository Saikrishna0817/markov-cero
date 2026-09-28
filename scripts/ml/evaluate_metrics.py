#!/usr/bin/env python3
"""Legacy evaluator for packed MCONNX01 ranker experiments.

The active W2 model is a bipartite GCN exported as standard ONNX; this script does
not evaluate that model. Current GCN train/validation/test metrics are emitted by
`train_branching_gnn.py`. An ONNX path passed here is rejected with an explanation.

Computes Kendall's tau (tau-a / tau-b), NDCG@10, NDCG@20 and MSE on the
fixed 70/15/15 instance split (Feature 26) for any MCONNX01 artifact,
without retraining.  The model is evaluated on RAW features exactly as the
C++ scorer consumes them (src/milp/ml_branching/onnx_scorer.cpp:91-98).

The featurization-order audit (Feature 27) runs first and aborts the
evaluation if the C++ writer/scorer orders diverge from the canonical order
or if the collected data violates the canonical feature domains.

Usage:
    python3 scripts/ml/evaluate_metrics.py --data data/ml_training \
        --model evidence/ml_models/branching_scorer_candidate.onnx \
        --out evidence/ml_model_evaluation.json
    # reuses a previously written split (same instance lists):
    python3 scripts/ml/evaluate_metrics.py --data data/ml_training \
        --split-json evidence/ml_dataset_split.json
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

import feature_spec  # noqa: E402
import ml_data  # noqa: E402
import ml_metrics  # noqa: E402
import ml_splits  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SHIPPED = REPO_ROOT / "data/ml_models/branching_scorer.onnx"


def _abs(path_str: str) -> Path:
    p = Path(path_str)
    return p if p.is_absolute() else REPO_ROOT / p


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data", default="data/ml_training")
    ap.add_argument("--model", default=None,
                    help="legacy MCONNX01 packed ranker (not standard ONNX)")
    ap.add_argument("--split-json", default=None,
                    help="reuse instance lists from a previous split report")
    ap.add_argument("--seed", type=int, default=ml_splits.DEFAULT_SEED)
    ap.add_argument("--split-strategy", default="auto",
                    choices=["auto", "hash", "count", "chronological"])
    ap.add_argument("--out", default="evidence/ml_model_evaluation.json")
    ap.add_argument("--skip-feature-audit", action="store_true")
    args = ap.parse_args()

    model_path = _abs(args.model) if args.model else DEFAULT_SHIPPED
    if not model_path.exists():
        print(f"model not found: {model_path}", file=sys.stderr)
        return 2
    if model_path.suffix.lower() == ".onnx":
        print("this legacy evaluator accepts only packed MCONNX01 rankers; "
              "standard ONNX GCN metrics are produced by train_branching_gnn.py",
              file=sys.stderr)
        return 2

    dataset = ml_data.load_dataset(_abs(args.data))
    instances = dataset["instances"]
    if not instances:
        print(f"no SB logs under {args.data}", file=sys.stderr)
        return 2

    all_feats = [X for e in instances.values() for X in e["X"]]
    audit = feature_spec.full_audit(np.vstack(all_feats))
    print(f"feature order audit: ok={audit['cpp_order_audit']['ok']} "
          f"scorer={audit['cpp_order_audit']['cpp_scorer_order']}")
    if not audit["cpp_order_audit"]["ok"]:
        print(f"FEATURE ORDER AUDIT FAILED: {audit['cpp_order_audit']['problems']}",
              file=sys.stderr)
        return 3
    if not audit["data_domain_audit"]["ok"] and not args.skip_feature_audit:
        print(f"emitted-data domain audit failed: "
              f"{audit['data_domain_audit']['violations']}", file=sys.stderr)
        return 4

    if args.split_json:
        split = json.loads(Path(_abs(args.split_json)).read_text())
        split["counts"] = ml_splits.split_report_with_data(split, instances)["counts"]
        split["reused_from"] = str(args.split_json)
        print(f"reusing split from {args.split_json}")
    else:
        split = ml_splits.split_instances(
            instances, seed=args.seed, strategy=args.split_strategy,
            timestamps=dataset["timestamps"],
        )
        split = ml_splits.split_report_with_data(split, instances)
        split["data_root"] = dataset["root"]
        split["generated_by"] = "scripts/ml/evaluate_metrics.py"
    ml_splits.print_split(split)

    model = ml_data.load_mconnx01(model_path)
    predict_fn = lambda X: ml_data.predict_mconnx01(model, X)  # noqa: E731

    results = {}
    for name in ml_splits.SPLIT_NAMES:
        recs = ml_data.iter_records(instances, split["instances"][name])
        results[name] = ml_metrics.evaluate_records(recs, predict_fn) if recs else {
            "nodes": 0, "candidates": 0, "note": "empty split"}

    report = {
        "generated_by": "scripts/ml/evaluate_metrics.py",
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "model": {"path": str(model_path), "bytes": model["bytes"],
                  "n_features": model["n_features"], "hidden": model["hidden"]},
        "dataset": {"root": dataset["root"],
                    **ml_data.dataset_stats(instances)},
        "split": {k: split.get(k) for k in
                  ("strategy", "seed", "ratios", "counts", "instances")},
        "feature_27_featurization": audit,
        "metrics": results,
        "definition": ml_metrics.METRICS_DEFINITION,
    }

    out = _abs(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2))

    for name in ml_splits.SPLIT_NAMES:
        m = results[name]
        if m.get("nodes", 0) == 0:
            print(f"  {name:<10} (empty)")
            continue
        print(f"  {name:<10} nodes {m['nodes']:>6}  tau_a "
              f"{m['kendall_tau_a']['mean']:+.4f}  tau_b "
              f"{m['kendall_tau_b']['mean']:+.4f}  ndcg@10 "
              f"{m['ndcg']['@10']:.4f}  ndcg@20 {m['ndcg']['@20']:.4f}  "
              f"mse {m['mse']:.6g}")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
