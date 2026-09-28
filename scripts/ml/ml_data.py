#!/usr/bin/env python3
"""Shared I/O for the ML branching pipeline.

The current training input is the solver's MCONLOG3 graph stream: per-record counts,
six-feature variable nodes, four-feature active-row nodes, candidate-to-row edges with
coefficient values, and aligned strong-branching scores. MCONLOG1 vector and MCONLOG2
three-row-feature graph records remain readable for legacy inspection; current training
rejects records without the MCONLOG3 row feature set.

The current candidate model is standard ONNX exported by `scripts/ml/export_onnx.py` and
read by the specialized fixed-architecture scorer in
`src/milp/ml_branching/onnx_scorer.cpp`. The MCONNX01 packed ranker helpers below are legacy
utilities retained for old experiments; they are not the current GCN export or C++ path.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

import numpy as np

from feature_spec import N_FEATURES

SB_MAGIC = b"MCONLOG1"
GRAPH_MAGIC_V2 = b"MCONLOG2"
GRAPH_MAGIC = b"MCONLOG3"
MODEL_MAGIC = b"MCONNX01"
HIDDEN = 16


def parse_sb_bytes(data: bytes) -> list[tuple[np.ndarray, np.ndarray]]:
    """Parse MCONLOG1/2/3 streams into [(features n x 6, scores n), ...].

    Records carry their own 8-byte magic (the solver appends across runs,
    src/milp/milp_solver.cpp:118-122).  Malformed or truncated tails are
    skipped so a killed collector never poisons the parsed prefix.
    """
    out: list[tuple[np.ndarray, np.ndarray]] = []
    if len(data) < 12:
        return out
    off = 0
    while off + 12 <= len(data):
        magic = data[off:off + 8]
        if magic not in (SB_MAGIC, GRAPH_MAGIC_V2, GRAPH_MAGIC):
            break  # corrupt tail; keep what we parsed so far
        off += 8
        if magic == SB_MAGIC:
            (n,) = struct.unpack_from("<I", data, off)
            off += 4
            nr = ne = 0
            row_width = 0
        else:
            n, nr, ne = struct.unpack_from("<III", data, off)
            off += 12
            row_width = 4 if magic == GRAPH_MAGIC else 3
        need = n * N_FEATURES * 8 + nr * row_width * 8 + ne * 16 + n * 8
        if n == 0 or off + need > len(data):
            break  # empty or truncated record
        feat = np.frombuffer(data, dtype="<f8", count=n * N_FEATURES, offset=off)
        feat = feat.reshape(n, N_FEATURES)
        off += n * N_FEATURES * 8
        off += nr * row_width * 8 + ne * 16  # graph payload; legacy API returns node features
        score = np.frombuffer(data, dtype="<f8", count=n, offset=off)
        off += n * 8
        out.append((feat, score))
    return out


def parse_graph_sb_bytes(data: bytes) -> list[dict]:
    """Parse graph logs without discarding constraint nodes or edge indices."""
    records: list[dict] = []
    off = 0
    while off + 12 <= len(data):
        magic = data[off:off + 8]
        off += 8
        if magic == SB_MAGIC:
            n = struct.unpack_from("<I", data, off)[0]
            off += 4
            nr = ne = 0
            row_width = 0
        elif magic in (GRAPH_MAGIC_V2, GRAPH_MAGIC):
            n, nr, ne = struct.unpack_from("<III", data, off)
            off += 12
            row_width = 4 if magic == GRAPH_MAGIC else 3
        else:
            break
        need = n * N_FEATURES * 8 + nr * row_width * 8 + ne * 16 + n * 8
        if n == 0 or off + need > len(data):
            break
        X = np.frombuffer(data, dtype="<f8", count=n * N_FEATURES, offset=off).copy().reshape(n, N_FEATURES)
        off += n * N_FEATURES * 8
        R = np.frombuffer(data, dtype="<f8", count=nr * row_width, offset=off).copy().reshape(nr, row_width)
        off += nr * row_width * 8
        edge_raw = np.frombuffer(data, dtype=[("v", "<u4"), ("r", "<u4"), ("a", "<f8")], count=ne, offset=off)
        edges = np.column_stack((edge_raw["v"], edge_raw["r"])).astype(np.int64, copy=True) if ne else np.empty((0, 2), dtype=np.int64)
        coefficients = edge_raw["a"].copy() if ne else np.empty((0,), dtype=np.float64)
        off += ne * 16
        S = np.frombuffer(data, dtype="<f8", count=n, offset=off).copy()
        off += n * 8
        records.append({"X": X, "R": R, "edges": edges,
                        "edge_coefficients": coefficients, "S": S})
    return records


def load_sb_log(path: Path | str) -> list[tuple[np.ndarray, np.ndarray]]:
    return [(rec["X"], rec["S"]) for rec in parse_graph_sb_bytes(Path(path).read_bytes())]


def load_graph_sb_log(path: Path | str) -> list[dict]:
    return parse_graph_sb_bytes(Path(path).read_bytes())


def instance_id_for(path: Path, root: Path) -> str:
    """Instance id of one .sb.bin file: first directory below the data root.

    collect_training_data.py writes <root>/<instance>/<instance>.sb.bin, so
    the directory name is the instance id; flat files fall back to the stem.
    """
    rel = path.resolve().relative_to(Path(root).resolve())
    if len(rel.parts) > 1:
        return rel.parts[0]
    return path.stem


def load_timestamps(root: Path) -> dict[str, float]:
    """instance_id -> epoch seconds from the collector's provenance.json.

    collect_training_data.py records `started_epoch` per instance; used for
    the chronological split (Feature 26).
    """
    prov_path = Path(root) / "provenance.json"
    if not prov_path.exists():
        return {}
    try:
        manifest = json.loads(prov_path.read_text())
    except (OSError, json.JSONDecodeError):
        return {}
    stamps: dict[str, float] = {}
    for rec in manifest.get("records", []):
        name = str(rec.get("instance", ""))
        epoch = rec.get("started_epoch")
        if not name or not isinstance(epoch, (int, float)):
            continue
        stem = Path(name).stem
        stamps[stem] = float(epoch)
        stamps[name] = float(epoch)
    return stamps


def load_dataset(root: Path | str) -> dict:
    """Load all instance logs under `root`.

    Returns {"root", "instances": {id: {"path", "X": [ndarray], "S": [ndarray]}},
             "timestamps": {id: epoch}, "paths": [...]}.
    """
    root = Path(root)
    instances: dict[str, dict] = {}
    paths = sorted(root.rglob("*.sb.bin"))
    for path in paths:
        records = load_graph_sb_log(path)
        if not records:
            continue
        iid = instance_id_for(path, root)
        entry = instances.setdefault(iid, {"path": str(path), "X": [], "R": [],
                                            "edges": [], "edge_coefficients": [], "S": []})
        entry["X"].extend(r["X"] for r in records)
        entry["R"].extend(r["R"] for r in records)
        entry["edges"].extend(r["edges"] for r in records)
        entry["edge_coefficients"].extend(r["edge_coefficients"] for r in records)
        entry["S"].extend(r["S"] for r in records)
    timestamps = load_timestamps(root)
    return {
        "root": str(root),
        "instances": instances,
        "timestamps": {k: v for k, v in timestamps.items() if k in instances},
        "paths": [str(p) for p in paths],
    }


def iter_records(instances: dict, ids) -> list[tuple[np.ndarray, np.ndarray]]:
    """Flatten selected instances into [(X, S), ...] node records."""
    out: list[tuple[np.ndarray, np.ndarray]] = []
    for iid in ids:
        entry = instances.get(iid)
        if entry is None:
            continue
        out.extend(zip(entry["X"], entry["S"]))
    return out


def iter_graph_records(instances: dict, ids) -> list[dict]:
    """Flatten selected instance ids while preserving each graph structure."""
    out = []
    for iid in ids:
        entry = instances.get(iid)
        if entry is None:
            continue
        for k, (X, R, E, A, S) in enumerate(zip(
                entry["X"], entry["R"], entry["edges"],
                entry["edge_coefficients"], entry["S"])):
            out.append({"X": X, "R": R, "edges": E,
                        "edge_coefficients": A, "S": S,
                        "instance": iid, "record_index": k})
    return out


def dataset_stats(instances: dict) -> dict:
    n_rec = sum(len(e["S"]) for e in instances.values())
    n_cand = sum(int(sum(len(s) for s in e["S"])) for e in instances.values())
    return {"instances": len(instances), "records": n_rec, "candidates": n_cand}


def load_mconnx01(path: Path | str) -> dict:
    """Load a MCONNX01 model (raw-feature space, as the C++ scorer does)."""
    path = Path(path)
    with open(path, "rb") as f:
        blob = f.read()
    if len(blob) < 16 or blob[:8] != MODEL_MAGIC:
        raise ValueError(f"bad model magic: {path}")
    n_features, hidden = struct.unpack_from("<II", blob, 8)
    if n_features != N_FEATURES:
        raise ValueError(f"unsupported n_features {n_features}")
    off = 16
    w1 = np.frombuffer(blob, dtype="<f4", count=hidden * n_features, offset=off)
    w1 = w1.reshape(hidden, n_features).astype(np.float64)
    off += hidden * n_features * 4
    b1 = np.frombuffer(blob, dtype="<f4", count=hidden, offset=off).astype(np.float64)
    off += hidden * 4
    w2s = np.frombuffer(blob, dtype="i1", count=hidden, offset=off).astype(np.float64)
    off += hidden
    (w2scale,) = struct.unpack_from("<f", blob, off)
    off += 4
    (b2,) = struct.unpack_from("<f", blob, off)
    return {
        "n_features": n_features,
        "hidden": hidden,
        "w1": w1,
        "b1": b1,
        "w2s": w2s,
        "w2scale": float(w2scale),
        "b2": float(b2),
        "path": str(path),
        "bytes": len(blob),
    }


def predict_mconnx01(model: dict, X: np.ndarray) -> np.ndarray:
    """Exact replica of OnnxBranchingScorer::score_candidates forward pass
    (src/milp/ml_branching/onnx_scorer.cpp:99-114), float64 instead of float32."""
    X = np.atleast_2d(np.asarray(X, dtype=np.float64))
    h = np.maximum(X @ model["w1"].T + model["b1"], 0.0)
    return h @ (model["w2scale"] * model["w2s"]) + model["b2"]
