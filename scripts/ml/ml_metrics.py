#!/usr/bin/env python3
"""Feature 28 — ML ranking & regression metrics (Kendall tau, NDCG@k, MSE).

Definitions (per node record = one ranked candidate list):

  Kendall's tau   tau-a, O(n^2) concordant/discordant pairs:
                      tau_a = (C - D) / (n (n - 1) / 2)
                  pairs tied on either side contribute to neither C nor D
                  (they stay in the denominator — tau-a tie handling).
                  tau-b (tie-corrected, matches scipy.stats.kendalltau's
                  default) is reported as well:
                      tau_b = (C - D) / sqrt((n0 - n1)(n0 - n2))
                  where n0 = n(n-1)/2, n1/n2 = tied pairs in pred/true.
                  Both are computed exactly in pure Python/NumPy — scipy is
                  optional and only used as a cross-check when importable.

  NDCG@k          DCG@k  = sum_{i=1..min(k,n)} gain_i / log2(i + 1)
                  IDCG@k = DCG of the gains sorted descending
                  gain   = the TRUE strong-branching score (Achterberg
                           product score, strong_branching.cpp:14-17; always
                           >= 0, infeasible down/up branches score ~1e8)
                  ideal ranking = true score descending
                  predicted ranking = predicted score descending (ties broken
                  by candidate index — same "highest score wins" rule the C++
                  solver uses, branch_selector.cpp:181-187)
                  k defaults: 10 and 20 (PROJECT.md feature 28).

  MSE             mean((predicted - true)^2) over every candidate of the split.

Aggregation: macro mean over node records (each B&B node weighs equally) plus
pooled/micro values where meaningful (tau over all pairs, MSE over all
candidates).  Records with < 2 candidates have no defined rank correlation and
are excluded from tau/NDCG node counts but still count toward MSE.
"""

from __future__ import annotations

import math
from typing import Callable, Sequence

import numpy as np

NDCG_KS = (10, 20)


def _as_arrays(pred, true) -> tuple[np.ndarray, np.ndarray]:
    p = np.asarray(pred, dtype=np.float64).ravel()
    t = np.asarray(true, dtype=np.float64).ravel()
    if p.shape != t.shape:
        raise ValueError(f"shape mismatch: pred {p.shape} vs true {t.shape}")
    return p, t


def _pair_signs(p: np.ndarray, t: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    iu = np.triu_indices(len(p), k=1)
    dp = np.sign(p[iu[0]] - p[iu[1]])
    dt = np.sign(t[iu[0]] - t[iu[1]])
    return dp, dt


def kendall_tau_a(pred, true) -> float:
    """Kendall tau-a: (C - D) / (n choose 2); tied pairs count as neither."""
    p, t = _as_arrays(pred, true)
    n = len(p)
    if n < 2:
        return float("nan")
    dp, dt = _pair_signs(p, t)
    prod = dp * dt
    c = int(np.count_nonzero(prod > 0))
    d = int(np.count_nonzero(prod < 0))
    return (c - d) / (n * (n - 1) / 2.0)


def kendall_tau_counts(pred, true) -> tuple[int, int, int]:
    """Return (concordant, discordant, tied_pairs) for pooled aggregation."""
    p, t = _as_arrays(pred, true)
    if len(p) < 2:
        return 0, 0, 0
    dp, dt = _pair_signs(p, t)
    prod = dp * dt
    c = int(np.count_nonzero(prod > 0))
    d = int(np.count_nonzero(prod < 0))
    ties = int(np.count_nonzero(prod == 0))
    return c, d, ties


def kendall_tau_b(pred, true) -> float:
    """Kendall tau-b (tie-corrected); equals scipy.stats.kendalltau default."""
    p, t = _as_arrays(pred, true)
    n = len(p)
    if n < 2:
        return float("nan")
    dp, dt = _pair_signs(p, t)
    prod = dp * dt
    c = int(np.count_nonzero(prod > 0))
    d = int(np.count_nonzero(prod < 0))
    n0 = n * (n - 1) / 2.0
    # tied pairs on each side (any tie, including ties on both sides)
    n1 = float(np.count_nonzero(dp == 0))
    n2 = float(np.count_nonzero(dt == 0))
    denom = math.sqrt(max(0.0, (n0 - n1) * (n0 - n2)))
    if denom <= 0.0:
        return float("nan")
    return (c - d) / denom


def dcg_at_k(gains: np.ndarray, k: int) -> float:
    m = min(k, len(gains))
    if m <= 0:
        return 0.0
    discounts = 1.0 / np.log2(np.arange(2, m + 2))
    return float(np.sum(gains[:m] * discounts))


def ndcg_at_k(pred, true, k: int) -> float:
    """NDCG@k with linear gain = true branch score (clipped at 0)."""
    p, t = _as_arrays(pred, true)
    n = len(t)
    if n == 0:
        return float("nan")
    gains = np.maximum(t, 0.0)
    order = np.argsort(-p, kind="stable")  # ties -> lower candidate index first
    dcg = dcg_at_k(gains[order], k)
    idcg = dcg_at_k(np.sort(gains)[::-1], k)
    if idcg <= 0.0:
        return 1.0  # nothing relevant to rank
    return dcg / idcg


def mse(pred, true) -> float:
    p, t = _as_arrays(pred, true)
    if len(p) == 0:
        return float("nan")
    return float(np.mean((p - t) ** 2))


def _mean(values: Sequence[float]) -> float:
    vals = [v for v in values if not math.isnan(v)]
    if not vals:
        return float("nan")
    return float(np.mean(vals))


def evaluate_records(
    records: Sequence[tuple[np.ndarray, np.ndarray]],
    predict_fn: Callable[[np.ndarray], np.ndarray],
    ks: Sequence[int] = NDCG_KS,
    cross_check_scipy: bool = True,
) -> dict:
    """Compute all Feature 28 metrics for one split.

    records   : [(n_i x 6 features, n_i true SB scores), ...]
    predict_fn: features -> predicted scores (raw feature space)
    """
    tau_a_vals: list[float] = []
    tau_b_vals: list[float] = []
    ndcg_vals: dict[int, list[float]] = {k: [] for k in ks}
    total_sse = 0.0
    total_n = 0
    pooled_c = pooled_d = pooled_pairs = 0
    n_nodes = n_rankable = 0
    score_min = math.inf
    score_max = -math.inf
    score_sum = 0.0
    n_negative = n_huge = 0
    scipy_deltas: list[float] = []

    for X, S in records:
        X = np.atleast_2d(np.asarray(X, dtype=np.float64))
        S = np.asarray(S, dtype=np.float64).ravel()
        if X.size == 0 or len(S) == 0:
            continue
        pred = np.asarray(predict_fn(X), dtype=np.float64).ravel()
        n_nodes += 1
        total_sse += float(np.sum((pred - S) ** 2))
        total_n += len(S)
        score_min = min(score_min, float(S.min()))
        score_max = max(score_max, float(S.max()))
        score_sum += float(S.sum())
        n_negative += int(np.count_nonzero(S < 0.0))
        n_huge += int(np.count_nonzero(S >= 1e6))

        c, d, ties = kendall_tau_counts(pred, S)
        pooled_c += c
        pooled_d += d
        pooled_pairs += c + d + ties
        if len(S) >= 2:
            n_rankable += 1
            tau_a_vals.append(kendall_tau_a(pred, S))
            tau_b_vals.append(kendall_tau_b(pred, S))
            for k in ks:
                ndcg_vals[k].append(ndcg_at_k(pred, S, k))
            if cross_check_scipy:
                try:
                    from scipy import stats as _sp_stats  # type: ignore

                    res = _sp_stats.kendalltau(pred, S)
                    if res.statistic is not None and not math.isnan(res.statistic):
                        scipy_deltas.append(abs(float(res.statistic) - tau_b_vals[-1]))
                except Exception:
                    cross_check_scipy = False

    tau_a_macro = _mean(tau_a_vals)
    tau_b_macro = _mean(tau_b_vals)
    denom = pooled_c + pooled_d
    tau_a_pooled = (pooled_c - pooled_d) / denom if denom else float("nan")

    metrics = {
        "nodes": n_nodes,
        "candidates": total_n,
        "rankable_nodes": n_rankable,
        "kendall_tau_a": {
            "mean": tau_a_macro,
            "pooled": tau_a_pooled,
            "definition": "(C - D) / (n choose 2) per node, macro-averaged; "
                          "pooled aggregates C/D over all nodes",
        },
        "kendall_tau_b": {
            "mean": tau_b_macro,
            "definition": "tie-corrected tau-b per node, macro-averaged "
                          "(scipy.stats.kendalltau equivalent)",
        },
        "ndcg": {f"@{k}": _mean(ndcg_vals[k]) for k in ks},
        "ndcg_k": list(ks),
        "mse": (total_sse / total_n) if total_n else float("nan"),
        "true_score_stats": {
            "min": score_min if total_n else None,
            "max": score_max if total_n else None,
            "mean": (score_sum / total_n) if total_n else None,
            "negative_gains": n_negative,
            "outlier_scores_ge_1e6": n_huge,
        },
        "pooled_pairs": pooled_pairs,
    }
    if scipy_deltas:
        metrics["scipy_cross_check"] = {
            "available": True,
            "max_abs_diff_tau_b": float(max(scipy_deltas)),
            "nodes_checked": len(scipy_deltas),
        }
    else:
        metrics["scipy_cross_check"] = {
            "available": False,
            "note": "scipy not installed; pure-NumPy tau-a/tau-b used "
                    "(validated offline against scipy)",
        }
    return metrics


METRICS_DEFINITION = {
    "kendall_tau_a": "rank correlation between predicted scores and true "
                     "strong-branching scores, per node, ties handled as "
                     "neither concordant nor discordant",
    "kendall_tau_b": "tie-corrected Kendall tau (scipy.stats.kendalltau default)",
    "ndcg@10": "DCG@10 / IDCG@10, gain = true branch score (linear), "
               "ranking by predicted score",
    "ndcg@20": "DCG@20 / IDCG@20, same definition",
    "mse": "mean squared error between predicted and true branch scores",
    "computed_on": "train / validation / test split at eval time",
}
