#!/usr/bin/env python3
"""Feature 26 — strict ML data partitioning + train-only normalizers.

Rules implemented (ORIGINAL_REQUEST.md R5 D-10):

  * Fixed instance-based 70% train / 15% validation / 15% test.  Splits are
    assigned at INSTANCE granularity: every node record of an instance stays
    in one split, so no B&B node of a test instance can leak into training
    (record-level random splits leak instance-specific structure).
  * Partitioning happens BEFORE any normalizer is fit: FeatureScaler.fit()
    is only ever called with training-split candidates.
  * Chronological split when instances carry timestamps/order (the collector
    writes `started_epoch` into provenance.json): earliest 70% -> train,
    next 15% -> validation, latest 15% -> test (no future leakage).
  * Deterministic (seeded) but STABLE per instance id: the default `hash`
    strategy buckets an instance with blake2b(f"{seed}:{instance_id}") and
    fixed thresholds, so membership depends only on (seed, instance id) —
    never on filesystem enumeration order, machine, or on which other
    instances happen to be in the dataset.  Re-running or appending data
    cannot reshuffle an instance's split.

Strategies (--split-strategy):
  auto           chronological when every instance has a timestamp; otherwise
                 `count` below 20 instances (exact ratios) and `hash` above
  hash           seeded hash-bucket thresholds (stable per instance id)
  count          exact 70/15/15 instance counts by seeded hash rank
  chronological  sort by timestamp, then exact 70/15/15 by position

Exact-count strategies use largest-remainder (Hamilton) apportionment of the
70/15/15 quotas; with n instances the counts are the closest integers to
(0.70n, 0.15n, 0.15n) and always sum to n.

Normalizers (FeatureScaler):
  zscore  x' = (x - mean) / std       fit on TRAIN candidates only
  robust  x' = (x - median) / IQR     fit on TRAIN candidates only; outlier
                                      resistant (ORIGINAL_REQUEST R5 asks
                                      explicitly for outlier handling)
  none    identity
  The affine FeatureScaler/fold_into_layer1 helpers below belong to the legacy
  MCONNX01 ranker. The current GCN exports train-only variable/row centers and
  scales as ONNX initializers; the C++ graph scorer applies those affine maps
  explicitly. Clipping/winsorizing is not used because it would need matching
  ONNX and C++ inference behavior.
"""

from __future__ import annotations

import hashlib
from typing import Iterable, Sequence

import numpy as np

SPLIT_RATIOS = {"train": 0.70, "validation": 0.15, "test": 0.15}
SPLIT_NAMES = ("train", "validation", "test")
DEFAULT_SEED = 0


def hash_key(seed: int, instance_id: str) -> float:
    """Deterministic value in [0, 1) derived from (seed, instance_id) only."""
    payload = f"{seed}:{instance_id}".encode("utf-8")
    digest = hashlib.blake2b(payload, digest_size=8).digest()
    return int.from_bytes(digest, "big") / float(1 << 64)


def _apportion(n: int) -> dict[str, int]:
    """Largest-remainder apportionment of the 70/15/15 quotas; sums to n."""
    quotas = {k: SPLIT_RATIOS[k] * n for k in SPLIT_NAMES}
    counts = {k: int(np.floor(q)) for k, q in quotas.items()}
    remainder = n - sum(counts.values())
    order = sorted(SPLIT_NAMES, key=lambda k: (-(quotas[k] - counts[k]), SPLIT_NAMES.index(k)))
    for k in order[:max(0, remainder)]:
        counts[k] += 1
    return counts


def split_instances(
    instance_ids: Iterable[str],
    seed: int = DEFAULT_SEED,
    strategy: str = "auto",
    timestamps: dict[str, float] | None = None,
) -> dict:
    """Assign each instance to train/validation/test. Returns a report dict."""
    ids = sorted(set(instance_ids))
    timestamps = timestamps or {}
    n = len(ids)
    missing = [i for i in ids if i not in timestamps]

    if strategy == "auto":
        if n > 0 and not missing:
            strategy = "chronological"
        elif n >= 20:
            # large enough for hash buckets to land close to 70/15/15 while
            # keeping membership stable per instance id
            strategy = "hash"
        else:
            # small instance counts: exact 70/15/15 counts matter more than
            # growth-stability, so apportion by seeded hash rank
            strategy = "count"
    if strategy == "chronological" and missing:
        raise ValueError(
            f"chronological split needs a timestamp for every instance; missing {missing}"
        )
    if strategy not in ("hash", "count", "chronological"):
        raise ValueError(f"unknown split strategy: {strategy}")

    members: dict[str, list[str]] = {k: [] for k in SPLIT_NAMES}
    if strategy == "hash":
        # Bucket thresholds: membership depends only on (seed, instance id).
        for iid in ids:
            u = hash_key(seed, iid)
            if u < SPLIT_RATIOS["train"]:
                members["train"].append(iid)
            elif u < SPLIT_RATIOS["train"] + SPLIT_RATIOS["validation"]:
                members["validation"].append(iid)
            else:
                members["test"].append(iid)
    elif strategy == "count":
        ranked = sorted(ids, key=lambda i: (hash_key(seed, i), i))
        counts = _apportion(n)
        pos = 0
        for name in SPLIT_NAMES:
            members[name] = ranked[pos:pos + counts[name]]
            pos += counts[name]
    else:  # chronological
        ordered = sorted(ids, key=lambda i: (timestamps[i], i))
        counts = _apportion(n)
        pos = 0
        for name in SPLIT_NAMES:
            members[name] = ordered[pos:pos + counts[name]]
            pos += counts[name]

    for name in SPLIT_NAMES:
        members[name].sort()

    report = {
        "strategy": strategy,
        "seed": seed,
        "ratios": dict(SPLIT_RATIOS),
        "instances": members,
        "counts": {k: len(v) for k, v in members.items()},
        "instance_ids": ids,
        "chronological": strategy == "chronological",
    }
    if strategy == "chronological":
        report["timestamps"] = {i: timestamps[i] for i in ids}
        ordered = sorted(ids, key=lambda i: (timestamps[i], i))
        report["time_order"] = ordered
        report["boundaries"] = {
            "train_end": timestamps[ordered[len(members["train"]) - 1]] if members["train"] else None,
            "validation_end": (
                timestamps[ordered[len(members["train"]) + len(members["validation"]) - 1]]
                if members["validation"] else None
            ),
        }
    return report


# Backlog item 7 freeze hook: the frozen instance manifests must keep using this
# module's hash buckets, so changing the bucketing here changes the manifests.
FREEZE_ALIASES = {"train": "train", "validation": "tune", "test": "holdout"}


def frozen_split(instance_ids, seed: int = DEFAULT_SEED) -> dict:
    """Deterministic 70/15/15 train/tune/holdout split for frozen manifests.

    Delegates to the `hash` strategy above, so membership depends only on
    (seed, instance id); split names are aliased to the roadmap's
    train/tune/holdout vocabulary.
    """
    report = split_instances(instance_ids, seed=seed, strategy="hash")
    return {
        "strategy": "hash",
        "seed": seed,
        "ratios": dict(SPLIT_RATIOS),
        "members": {FREEZE_ALIASES[name]: report["instances"][name]
                    for name in SPLIT_NAMES},
        "counts": {FREEZE_ALIASES[name]: report["counts"][name]
                   for name in SPLIT_NAMES},
        "instance_ids": report["instance_ids"],
    }


class FeatureScaler:
    """Affine feature normalizer fit on TRAINING candidates only."""

    def __init__(self, kind: str = "zscore"):
        if kind not in ("zscore", "robust", "none"):
            raise ValueError(f"unknown scaler kind: {kind}")
        self.kind = kind
        self.center: np.ndarray | None = None
        self.scale: np.ndarray | None = None
        self.n_fit_rows = 0
        self.fit_on = "unfit"

    def fit(self, X: np.ndarray, fit_on: str = "train") -> "FeatureScaler":
        X = np.atleast_2d(np.asarray(X, dtype=np.float64))
        self.n_fit_rows = int(X.shape[0])
        self.fit_on = fit_on
        if self.kind == "none":
            self.center = np.zeros(X.shape[1])
            self.scale = np.ones(X.shape[1])
            return self
        if self.kind == "zscore":
            self.center = X.mean(axis=0)
            std = X.std(axis=0)
            self.scale = np.where(std > 1e-12, std, 1.0)
        else:  # robust
            self.center = np.median(X, axis=0)
            q75, q25 = np.percentile(X, [75, 25], axis=0)
            iqr = q75 - q25
            std = X.std(axis=0)
            self.scale = np.where(iqr > 1e-12, iqr, np.where(std > 1e-12, std, 1.0))
        return self

    def transform(self, X: np.ndarray) -> np.ndarray:
        if self.center is None:
            raise RuntimeError("scaler not fit")
        X = np.asarray(X, dtype=np.float64)
        return (X - self.center) / self.scale

    def state_dict(self) -> dict:
        return {
            "kind": self.kind,
            "fit_on": self.fit_on,
            "n_fit_rows": self.n_fit_rows,
            "center": None if self.center is None else [float(v) for v in self.center],
            "scale": None if self.scale is None else [float(v) for v in self.scale],
        }

    @classmethod
    def from_state(cls, state: dict) -> "FeatureScaler":
        sc = cls(state["kind"])
        sc.center = np.asarray(state["center"], dtype=np.float64)
        sc.scale = np.asarray(state["scale"], dtype=np.float64)
        sc.fit_on = state.get("fit_on", "train")
        sc.n_fit_rows = int(state.get("n_fit_rows", 0))
        return sc


def fold_into_layer1(
    w1: np.ndarray, b1: np.ndarray, scaler: FeatureScaler
) -> tuple[np.ndarray, np.ndarray]:
    """Fold x' = (x - c)/s into layer 1 so RAW features give identical output.

    relu(W1 x' + b1) == relu((W1/s) x + (b1 - (W1/s) c))
    The exported MCONNX01 therefore stays compatible with the raw-feature
    C++ inference path (onnx_scorer.cpp:91-98) without any runtime scaler.
    """
    if scaler.kind == "none" or scaler.center is None:
        return np.asarray(w1, dtype=np.float64), np.asarray(b1, dtype=np.float64)
    w1_new = np.asarray(w1, dtype=np.float64) / scaler.scale[None, :]
    b1_new = np.asarray(b1, dtype=np.float64) - w1_new @ scaler.center
    return w1_new, b1_new


def split_report_with_data(
    split: dict,
    instances: dict[str, dict],
    counts_fn=None,
) -> dict:
    """Attach per-split record/candidate counts (needs ml_data lazily)."""
    import ml_data

    counts_fn = counts_fn or ml_data.dataset_stats
    out = dict(split)
    out["counts"] = {
        name: counts_fn({i: instances[i] for i in split["instances"][name] if i in instances})
        for name in SPLIT_NAMES
    }
    present = {i for name in SPLIT_NAMES for i in split["instances"][name]}
    out["excluded_instances"] = sorted(set(instances) - present)
    return out


def print_split(split: dict) -> None:
    print(f"split: strategy={split['strategy']} seed={split['seed']} "
          f"(train {SPLIT_RATIOS['train']:.0%} / validation {SPLIT_RATIOS['validation']:.0%} / "
          f"test {SPLIT_RATIOS['test']:.0%})")
    for name in SPLIT_NAMES:
        c = split["counts"][name]
        ids = split["instances"][name]
        print(f"  {name:<10} instances {len(ids):>4}  records {c['records']:>7}  "
              f"candidates {c['candidates']:>8}")
    if split.get("excluded_instances"):
        print(f"  excluded (no records): {', '.join(split['excluded_instances'])}")
