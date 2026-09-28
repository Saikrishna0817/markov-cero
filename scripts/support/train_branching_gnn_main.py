from __future__ import annotations
from .train_branching_gnn_config import (
    HIDDEN, Path, ROOT, argparse, feature_spec, hashlib, json, ml_data, ml_splits, np, sys, time, torch
)
from .train_branching_gnn_BipartiteGCN import BipartiteGCN
from .train_branching_gnn_BipartiteGCN import evaluate
from .train_branching_gnn_BipartiteGCN import graph_tensors
from .train_branching_gnn_BipartiteGCN import normalized_edges
from .train_branching_gnn_BipartiteGCN import predict_records
from .train_branching_gnn_BipartiteGCN import rank_loss

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data", default="data/ml_training")
    ap.add_argument("--out", default="evidence/ml_models/branching_gcn_candidate.onnx")
    ap.add_argument("--checkpoint-out", default="evidence/ml_models/branching_gcn_candidate.pt")
    ap.add_argument("--metrics-out", default="evidence/ml_training_metrics.json")
    ap.add_argument("--split-out", default="evidence/ml_dataset_split.json")
    ap.add_argument("--epochs", type=int, default=50)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--batch-size", type=int, default=32)
    ap.add_argument("--seed", type=int, default=ml_splits.DEFAULT_SEED)
    ap.add_argument("--split-strategy", default="auto",
                    choices=["auto", "hash", "count", "chronological"])
    ap.add_argument("--min-records", type=int, default=10)
    args = ap.parse_args()

    if args.epochs != 50:
        print("W2 acceptance training requires exactly 50 epochs", file=sys.stderr)
        return 2
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    root = Path(args.data)
    dataset = ml_data.load_dataset(root)
    instances = dataset["instances"]
    X_all = [X for ent in instances.values() for X in ent["X"]]
    if not X_all:
        print("no self-collected SB graph records; run collect_training_data.py", file=sys.stderr)
        return 2
    order_audit = feature_spec.audit_cpp_order()
    domain_audit = feature_spec.audit_feature_matrix(np.vstack(X_all))
    if not order_audit["ok"] or not domain_audit["ok"]:
        print("feature order/domain audit failed; refusing training", file=sys.stderr)
        return 3
    stats = ml_data.dataset_stats(instances)
    if stats["records"] < args.min_records:
        print(f"too few SB node records: {stats['records']} < {args.min_records}", file=sys.stderr)
        return 2

    split = ml_splits.split_instances(instances, seed=args.seed,
                                      strategy=args.split_strategy,
                                      timestamps=dataset["timestamps"])
    split = ml_splits.split_report_with_data(split, instances)
    if not split["instances"]["train"] or not split["instances"]["validation"] \
            or not split["instances"]["test"]:
        print("need disjoint train, validation, and test instances", file=sys.stderr)
        return 2
    split["data_root"] = str(root)
    split["generated_by"] = "scripts/ml/train_branching_gnn.py"
    split["feature_order"] = list(feature_spec.FEATURE_ORDER)
    tr = ml_data.iter_graph_records(instances, split["instances"]["train"])
    va = ml_data.iter_graph_records(instances, split["instances"]["validation"])
    te = ml_data.iter_graph_records(instances, split["instances"]["test"])
    if not tr or not va:
        print("empty graph train/validation records", file=sys.stderr)
        return 2
    graph_problems = []
    for rec in tr + va + te:
        X, R, E, A, S = (rec[k] for k in ("X", "R", "edges", "edge_coefficients", "S"))
        if X.ndim != 2 or X.shape[1] != 6 or len(S) != len(X):
            graph_problems.append("variable/score shape mismatch")
        if R.ndim != 2 or R.shape[1] != 4:
            graph_problems.append("expected four row features per active row")
        if E.shape != (len(A), 2):
            graph_problems.append("edge index/coefficient shape mismatch")
        elif len(E) and (E[:, 0].max() >= len(X) or E[:, 1].max() >= len(R) or E.min() < 0):
            graph_problems.append("edge endpoint out of range")
        if not all(np.isfinite(a).all() for a in (X, R, A, S)):
            graph_problems.append("non-finite graph data")
    if graph_problems:
        print("graph data audit failed: " + "; ".join(sorted(set(graph_problems))), file=sys.stderr)
        return 3

    # Fit both variable and row scalers on the training instances only.
    x_center = np.median(np.vstack([r["X"] for r in tr]), axis=0)
    x_iqr = np.percentile(np.vstack([r["X"] for r in tr]), 75, axis=0) - np.percentile(np.vstack([r["X"] for r in tr]), 25, axis=0)
    x_scale = np.where(x_iqr > 1e-12, x_iqr, 1.0)
    train_rows = [r["R"] for r in tr if len(r["R"])]
    r_raw = np.vstack(train_rows) if train_rows else np.zeros((1, 3))
    r_center = np.median(r_raw, axis=0)
    r_iqr = np.percentile(r_raw, 75, axis=0) - np.percentile(r_raw, 25, axis=0)
    r_scale = np.where(r_iqr > 1e-12, r_iqr, 1.0)
    scalers = (x_center, x_scale, r_center, r_scale)

    model = BipartiteGCN()
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)
    rng = np.random.default_rng(args.seed)
    history = []
    for epoch in range(args.epochs):
        model.train()
        order = rng.permutation(len(tr))
        losses = []
        for start in range(0, len(order), args.batch_size):
            optimizer.zero_grad()
            batch = [tr[i] for i in order[start:start + args.batch_size]]
            losses_batch = []
            for rec in batch:
                pred = model(*graph_tensors(rec, *scalers))
                target = torch.as_tensor(rec["S"], dtype=torch.float32)
                losses_batch.append(rank_loss(pred, target))
            loss = torch.stack(losses_batch).mean()
            loss.backward()
            optimizer.step()
            losses.append(float(loss.detach()))
        tr_pred = predict_records(model, tr, scalers)
        va_pred = predict_records(model, va, scalers)
        mtr = evaluate(tr, tr_pred)
        mva = evaluate(va, va_pred)
        history.append({"epoch": epoch + 1, "train_loss": float(np.mean(losses)),
                        "train": mtr, "validation": mva})
        print(f"epoch {epoch + 1}/50 train_loss={np.mean(losses):.6g} "
              f"train_tau={mtr['kendall_tau_a']['mean']:+.4f} "
              f"validation_tau={mva['kendall_tau_a']['mean']:+.4f} "
              f"validation_ndcg@10={mva['ndcg']['@10']:.4f}")

    out = Path(args.out)
    if not out.is_absolute():
        out = ROOT / out
    out.parent.mkdir(parents=True, exist_ok=True)
    example = next(r for r in tr if len(r["R"]) > 0)
    scaled_example = graph_tensors(example, *scalers)
    edge_index, edge_weight = normalized_edges(example)
    checkpoint_path = Path(args.checkpoint_out)
    if not checkpoint_path.is_absolute(): checkpoint_path = ROOT / checkpoint_path
    checkpoint_path.parent.mkdir(parents=True, exist_ok=True)
    torch.save({"state_dict": model.state_dict(),
                "scalers": [x.tolist() for x in scalers],
                "scaled_example": scaled_example,
                "example": {"X": example["X"], "R": example["R"],
                            "edges": edge_index, "edge_weights": edge_weight}},
               checkpoint_path)
    from export_onnx import export_checkpoint
    try:
        onnx_report = export_checkpoint(checkpoint_path, out)
    except Exception as exc:
        print(f"ONNX export/parity failed: {exc}", file=sys.stderr)
        return 4
    error = onnx_report["onnxruntime_max_abs_error"]

    te_pred = predict_records(model, te, scalers) if te else []
    final = {name: evaluate(recs, preds) for name, recs, preds in
             (("train", tr, tr_pred), ("validation", va, va_pred), ("test", te, te_pred))}
    split_path = Path(args.split_out)
    if not split_path.is_absolute(): split_path = ROOT / split_path
    split_path.parent.mkdir(parents=True, exist_ok=True)
    split_path.write_text(json.dumps(split, indent=2))
    metrics = {
        "generated_by": "scripts/ml/train_branching_gnn.py",
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "architecture": {"name": "two-layer bipartite GCN", "hidden": HIDDEN,
                         "variable_features": list(feature_spec.FEATURE_ORDER),
                         "row_features": ["normalized_rhs", "normalized_activity",
                                          "normalized_dual_multiplier", "row_density"],
                         "edge": "active-row CSC coefficient, symmetric degree normalization"},
        "split": split,
        "scalers_train_only": {"variable_center": x_center.tolist(), "variable_scale": x_scale.tolist(),
                               "row_center": r_center.tolist(), "row_scale": r_scale.tolist()},
        "epochs": args.epochs, "learning_rate": args.lr, "batch_size": args.batch_size,
        "records": stats["records"], "instances": stats["instances"],
        "feature_order_audit": order_audit, "feature_domain_audit": domain_audit,
        "history": history, "metrics": final,
        "onnx": {"path": str(out), "bytes": out.stat().st_size,
                 "sha256": hashlib.sha256(out.read_bytes()).hexdigest(),
                 "checker": "passed", "onnxruntime_max_abs_error": error,
                 "checkpoint": str(checkpoint_path)},
        "limitations": ["D-18 int8 quantization is not yet applied; candidate ONNX stays unshipped until C++ parity and node-count gates pass."],
    }
    metrics_path = Path(args.metrics_out)
    if not metrics_path.is_absolute(): metrics_path = ROOT / metrics_path
    metrics_path.parent.mkdir(parents=True, exist_ok=True)
    metrics_path.write_text(json.dumps(metrics, indent=2))
    print(f"exported valid ONNX: {out} ({out.stat().st_size} bytes), max_abs_error={error:.3g}")
    return 0
