#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.support.train_branching_gnn_BipartiteGCN import BipartiteGCN
from scripts.support.train_branching_gnn_BipartiteGCN import RawInputExport
from scripts.support.train_branching_gnn_BipartiteGCN import normalized_edges
from scripts.support.train_branching_gnn_BipartiteGCN import graph_tensors
from scripts.support.train_branching_gnn_BipartiteGCN import rank_loss
from scripts.support.train_branching_gnn_BipartiteGCN import predict_records
from scripts.support.train_branching_gnn_BipartiteGCN import evaluate
from scripts.support.train_branching_gnn_main import main
from scripts.support.train_branching_gnn_config import (
    HIDDEN, Path, ROOT, _ENTRY_POINT, _EntryPath, argparse, feature_spec, hashlib, json, ml_data, ml_metrics, ml_splits, nn, np, onnx, ort, sys, time, torch
)

if __name__ == "__main__":
    raise SystemExit(main())
