#!/usr/bin/env python3
"""Feature 27 — bipartite graph featurization: canonical feature order + audits.

The C++ inference path is the GROUND TRUTH.  Everything in scripts/ml/ must
emit and consume features in exactly the order the scorer consumes them.

CANONICAL PER-CANDIDATE FEATURE VECTOR (n_features = 6)
-------------------------------------------------------
  idx  name                      C++ struct field            citations
  ---  ------------------------  --------------------------  ---------------------------------------------
   0   fractionality             NodeFeatureVector.fractionality
                                 scorer:  src/milp/ml_branching/onnx_scorer.cpp:266
                                 logger:  src/milp/ml_branching/onnx_scorer.cpp:386
                                 struct:  include/markov_cero/milp/branch_selector.hpp:30
   1   objective_coefficient     .objective_coefficient
                                 scorer:  src/milp/ml_branching/onnx_scorer.cpp:266
                                 logger:  src/milp/ml_branching/onnx_scorer.cpp:386
                                 struct:  include/markov_cero/milp/branch_selector.hpp:31
   2   pseudocost_down_ratio     .pseudocost_down_ratio
                                 scorer:  src/milp/ml_branching/onnx_scorer.cpp:267
                                 logger:  src/milp/ml_branching/onnx_scorer.cpp:387
                                 struct:  include/markov_cero/milp/branch_selector.hpp:32
   3   pseudocost_up_ratio       .pseudocost_up_ratio
                                 scorer:  src/milp/ml_branching/onnx_scorer.cpp:267
                                 logger:  src/milp/ml_branching/onnx_scorer.cpp:387
                                 struct:  include/markov_cero/milp/branch_selector.hpp:33
   4   bound_width               .bound_width
                                 scorer:  src/milp/ml_branching/onnx_scorer.cpp:267
                                 logger:  src/milp/ml_branching/onnx_scorer.cpp:387
                                 struct:  include/markov_cero/milp/branch_selector.hpp:34
   5   column_density            .column_density
                                 scorer:  src/milp/ml_branching/onnx_scorer.cpp:267
                                 logger:  src/milp/ml_branching/onnx_scorer.cpp:387
                                 struct:  include/markov_cero/milp/branch_selector.hpp:35

The graph scorer input assembly and MCONLOG3 writer list the same six
variable fields in the same order; audit_cpp_order() parses both C++ blocks
and fails if their order diverges from FEATURE_ORDER below.

BIPARTITE GRAPH SEMANTICS (constraint / variable / edge representations)
------------------------------------------------------------------------
The MCONLOG3 record stores variable nodes, active constraint-row nodes, and
candidate-to-row edges:

  * VARIABLE node representation  = the 6-vector above (one per candidate).
  * Variable ORDER inside a record = ascending column index: candidates come
    from find_fractional_variables(), which iterates j = 0..n-1
    (src/milp/branch_selector.cpp:49-63), and the SB logging path evaluates
    exactly that same ascending list (strong_branching.cpp:27-32 with
    StrongBranchingOptions.max_candidates == 0, the default — so the
    fractionality sort at strong_branching.cpp:38-49 never triggers, and
    features[k] zips with sb_scores[k] element-wise, see
    src/milp/ml_branching/onnx_scorer.cpp:144-148).
  * CONSTRAINT node representation = row order of the model matrix. The row
    vector stores normalized side, normalized primal activity, normalized
    dual multiplier, and row density.
  * EDGE representation            = CSC nonzero pattern restricted to rows
    active at this relaxation; each edge carries its A_ij coefficient.

Domain constraints that follow from how the C++ extracts each field (used by
audit_feature_matrix to DETECT an ordering bug in emitted data):
  * fractionality      in [0, 0.5]                 (branch_selector.cpp:235)
  * pseudocost_down_ratio + pseudocost_up_ratio == 1
                       (branch_selector.cpp:245-246: both are x/denom or
                        0.5/0.5 when denom == 0)
  * bound_width        >= 0                        (branch_selector.cpp:248-254)
  * column_density     >= 0                        (branch_selector.cpp:260-262)
  * objective_coefficient is unbounded (signed) -> no range check
"""

from __future__ import annotations

import re
from pathlib import Path

import numpy as np

N_FEATURES = 6

#: The one and only feature order accepted anywhere in this pipeline.
FEATURE_ORDER: tuple[str, ...] = (
    "fractionality",
    "objective_coefficient",
    "pseudocost_down_ratio",
    "pseudocost_up_ratio",
    "bound_width",
    "column_density",
)

FEATURE_CITATIONS: dict[str, dict[str, str]] = {
    "fractionality": {
        "scorer": "src/milp/ml_branching/onnx_scorer.cpp:266",
        "logger": "src/milp/ml_branching/onnx_scorer.cpp:386",
        "struct": "include/markov_cero/milp/branch_selector.hpp:30",
        "extract": "src/milp/branch_selector.cpp:256",
    },
    "objective_coefficient": {
        "scorer": "src/milp/ml_branching/onnx_scorer.cpp:266",
        "logger": "src/milp/ml_branching/onnx_scorer.cpp:386",
        "struct": "include/markov_cero/milp/branch_selector.hpp:31",
        "extract": "src/milp/branch_selector.cpp:257",
    },
    "pseudocost_down_ratio": {
        "scorer": "src/milp/ml_branching/onnx_scorer.cpp:267",
        "logger": "src/milp/ml_branching/onnx_scorer.cpp:387",
        "struct": "include/markov_cero/milp/branch_selector.hpp:32",
        "extract": "src/milp/branch_selector.cpp:266",
    },
    "pseudocost_up_ratio": {
        "scorer": "src/milp/ml_branching/onnx_scorer.cpp:267",
        "logger": "src/milp/ml_branching/onnx_scorer.cpp:387",
        "struct": "include/markov_cero/milp/branch_selector.hpp:33",
        "extract": "src/milp/branch_selector.cpp:267",
    },
    "bound_width": {
        "scorer": "src/milp/ml_branching/onnx_scorer.cpp:267",
        "logger": "src/milp/ml_branching/onnx_scorer.cpp:387",
        "struct": "include/markov_cero/milp/branch_selector.hpp:34",
        "extract": "src/milp/branch_selector.cpp:275",
    },
    "column_density": {
        "scorer": "src/milp/ml_branching/onnx_scorer.cpp:267",
        "logger": "src/milp/ml_branching/onnx_scorer.cpp:387",
        "struct": "include/markov_cero/milp/branch_selector.hpp:35",
        "extract": "src/milp/branch_selector.cpp:281",
    },
}


def repo_root() -> Path:
    """scripts/ml/feature_spec.py -> repository root."""
    return Path(__file__).resolve().parents[2]


def _extract_block(cpp_text: str, decl_pattern: str) -> list[str]:
    """Return the field names inside the first `decl_pattern = { ... };` block."""
    m = re.search(decl_pattern + r"\s*=\s*\{(.*?)\};", cpp_text, re.S)
    if not m:
        return []
    return re.findall(r"(?:features\[k\]\.|f\.)(\w+)", m.group(1))


def cpp_source_feature_order(root: Path | None = None) -> dict[str, list[str]]:
    """Re-read the C++ ground truth and return both feature orders found there.

    scorer = score_graph() variable feature assembly
    logger = append_sb_record() MCONLOG3 writer
    """
    root = Path(root) if root is not None else repo_root()
    cpp = (root / "src/milp/ml_branching/onnx_scorer.cpp").read_text()
    scorer = _extract_block(cpp, r"const double raw\[kVarFeatures\]")
    logger = _extract_block(cpp, r"const double values\[kVarFeatures\]")
    return {"scorer": scorer, "logger": logger}


def audit_cpp_order(root: Path | None = None) -> dict:
    """Assert scorer order == logger order == FEATURE_ORDER (Feature 27)."""
    found = cpp_source_feature_order(root)
    problems = []
    if tuple(found["scorer"]) != FEATURE_ORDER:
        problems.append(f"scorer order {found['scorer']} != FEATURE_ORDER")
    if tuple(found["logger"]) != FEATURE_ORDER:
        problems.append(f"logger order {found['logger']} != FEATURE_ORDER")
    if found["scorer"] != found["logger"]:
        problems.append("scorer order != logger order (train/inference skew)")
    if not found["scorer"] or not found["logger"]:
        problems.append("could not parse onnx_scorer.cpp feature blocks")
    return {
        "ok": not problems,
        "canonical": list(FEATURE_ORDER),
        "cpp_scorer_order": found["scorer"],
        "cpp_logger_order": found["logger"],
        "cpp_source": "src/milp/ml_branching/onnx_scorer.cpp",
        "problems": problems,
    }


def audit_feature_matrix(X: np.ndarray, atol_ratio: float = 1e-6) -> dict:
    """Domain-check an (n, 6) feature matrix in CANONICAL order.

    Catches a column-order BUG in data emitted by the collector: e.g. if the
    logger wrote columns in a different order than the scorer consumes, the
    ratio pair (idx 2/3) would no longer sum to 1 and range checks would fire.
    """
    X = np.atleast_2d(np.asarray(X, dtype=np.float64))
    report: dict = {"rows": int(X.shape[0]), "ok": True, "violations": {}}
    if X.shape[1] != N_FEATURES:
        report["ok"] = False
        report["violations"]["shape"] = f"expected {N_FEATURES} columns, got {X.shape[1]}"
        return report

    finite = np.isfinite(X)
    if not finite.all():
        report["violations"]["nonfinite"] = int((~finite).sum())

    frac = X[:, 0]
    bad = int(np.count_nonzero((frac < -1e-9) | (frac > 0.5 + 1e-9)))
    if bad:
        report["violations"]["fractionality_out_of_range"] = bad

    ratio_sum = X[:, 2] + X[:, 3]
    tol = atol_ratio * max(1.0, float(np.max(np.abs(ratio_sum)) if len(ratio_sum) else 1.0))
    bad = int(np.count_nonzero(np.abs(ratio_sum - 1.0) > 1e-6))
    if bad:
        report["violations"]["pseudocost_ratios_not_summing_to_1"] = bad

    bad = int(np.count_nonzero(X[:, 4] < -1e-9))
    if bad:
        report["violations"]["negative_bound_width"] = bad

    bad = int(np.count_nonzero(X[:, 5] < -1e-9))
    if bad:
        report["violations"]["negative_column_density"] = bad

    report["ok"] = not report["violations"]
    report["column_ranges"] = {
        name: [float(X[:, i].min()), float(X[:, i].max())] for i, name in enumerate(FEATURE_ORDER)
    }
    report["ratio_sum_max_abs_error"] = float(np.max(np.abs(ratio_sum - 1.0))) if len(ratio_sum) else 0.0
    report["tolerance_note"] = (
        f"ratio-sum tolerance {tol:.3g}; range checks use ±1e-9 slack"
    )
    return report


def full_audit(X: np.ndarray | None = None, root: Path | None = None) -> dict:
    """C++ source order audit + (optional) emitted-data domain audit."""
    report = {"cpp_order_audit": audit_cpp_order(root)}
    if X is not None:
        report["data_domain_audit"] = audit_feature_matrix(X)
    report["ok"] = report["cpp_order_audit"]["ok"] and (
        report.get("data_domain_audit", {"ok": True})["ok"]
    )
    return report


if __name__ == "__main__":
    import json
    import sys

    res = full_audit()
    print(json.dumps(res, indent=2))
    sys.exit(0 if res["ok"] else 1)
