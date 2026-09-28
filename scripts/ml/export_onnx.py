#!/usr/bin/env python3
"""Export a trained bipartite GCN checkpoint as a checked standard ONNX file."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
import onnx
import onnxruntime as ort
import torch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from train_branching_gnn import BipartiteGCN, RawInputExport


def export_checkpoint(checkpoint_path: Path | str, output_path: Path | str) -> dict:
    checkpoint_path, output_path = Path(checkpoint_path), Path(output_path)
    payload = torch.load(checkpoint_path, map_location="cpu", weights_only=False)
    model = BipartiteGCN()
    model.load_state_dict(payload["state_dict"])
    scalers = tuple(np.asarray(x, dtype=np.float32) for x in payload["scalers"])
    wrapper = RawInputExport(model.eval(), scalers).eval()
    sample = payload["example"]
    x = torch.as_tensor(sample["X"], dtype=torch.float32)
    rows = torch.as_tensor(sample["R"], dtype=torch.float32)
    edges = torch.as_tensor(sample["edges"], dtype=torch.long)
    weights = torch.as_tensor(sample["edge_weights"], dtype=torch.float32)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    torch.onnx.export(
        wrapper, (x, rows, edges, weights), output_path,
        input_names=["variable_features", "row_features", "edge_index", "edge_weight"],
        output_names=["branch_scores"], opset_version=18,
        dynamic_axes={"variable_features": {0: "n_variables"},
                      "row_features": {0: "n_rows"},
                      "edge_index": {1: "n_edges"},
                      "edge_weight": {0: "n_edges"},
                      "branch_scores": {0: "n_variables"}},
        dynamo=False,
    )
    onnx.checker.check_model(onnx.load(output_path))
    session = ort.InferenceSession(str(output_path), providers=["CPUExecutionProvider"])
    edge_index = edges.numpy()
    edge_weight = weights.numpy()
    actual = session.run(None, {"variable_features": x.numpy(), "row_features": rows.numpy(),
                                "edge_index": edge_index, "edge_weight": edge_weight})[0].ravel()
    with torch.no_grad():
        reference = model(*payload["scaled_example"]).cpu().numpy().ravel()
    error = float(np.max(np.abs(actual - reference))) if len(actual) else 0.0
    # ONNX Runtime and PyTorch may accumulate the same FP32 scatter reductions
    # in different orders. Keep parity strict at 1e-4 absolute while allowing
    # the expected last-bit differences from those reductions.
    if not np.all(np.isfinite(actual)) or error > 1e-4:
        raise RuntimeError(f"ONNX runtime parity failed: max_abs_error={error}")
    report = {"path": str(output_path), "bytes": output_path.stat().st_size,
              "checker": "passed", "onnxruntime_max_abs_error": error}
    output_path.with_suffix(output_path.suffix + ".provenance.json").write_text(
        json.dumps(report, indent=2))
    return report


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--checkpoint", required=True, help="PyTorch checkpoint from GCN training")
    ap.add_argument("--out", default="evidence/ml_models/branching_gcn_candidate.onnx")
    args = ap.parse_args()
    try:
        report = export_checkpoint(args.checkpoint, args.out)
    except Exception as exc:
        print(f"ONNX export failed: {exc}", file=sys.stderr)
        return 2
    print(f"exported standard ONNX {report['path']} ({report['bytes']} bytes); "
          f"max_abs_error={report['onnxruntime_max_abs_error']:.3g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
