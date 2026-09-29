#!/usr/bin/env python3
"""Frozen timing definitions for the benchmark/compare harnesses (backlog item 7).

Single source of truth imported by both harness configs
(`run_full_compare_config.py`, `run_full_benchmark_config.py`) and by the
harness entry points (`run_compare.py`, `run_netlib.py`, `run_miplib.py`).

Frozen here:
  * per-solve time caps  LP/QP 60 s, MIP 300 s, release MIP 3600 s
  * at least 5 repeats per reported cell
  * timing scope: process-wall (parent observed) vs engine-only
  * cold vs repeat: the first solve of an instance is cold, later ones warm

`DEFAULTS_STATUS` records, for every constant, the harness default it feeds and
whether that default is KEPT (matches the roadmap target) or a documented
DEVIATION. Nothing here is tuned; these are the definitions used to regenerate
the pre-tuning baseline.
"""
from __future__ import annotations

FROZEN_ON = "2026-09-28"

LP_TIME_LIMIT_S = 60.0
QP_TIME_LIMIT_S = 60.0
MIP_TIME_LIMIT_S = 300.0
RELEASE_MIP_TIME_LIMIT_S = 3600.0
MIN_REPEATS = 5
MATCHED_THREADS = 1

# Per-class cap for one solve of one instance by one solver.
CLASS_TIME_LIMITS = {
    "LP": LP_TIME_LIMIT_S,
    "QP": QP_TIME_LIMIT_S,
    "MILP": MIP_TIME_LIMIT_S,
}

# Suite -> problem class used to resolve a default cap when --timeout is unset.
SUITE_CLASS = {
    "netlib": "LP",
    "qplib": "QP",
    "miplib": "MILP",
    "mittelmann": "MILP",
    "cases": "MILP",
}

TIMING_SCOPE = {
    "process_wall": (
        "Parent-observed wall clock around the solver process, including model "
        "parse, CLI start-up and result serialisation."
    ),
    "engine_only": (
        "Time reported by the solver engine itself, excluding parse/start-up; "
        "not comparable across solvers and never mixed into cross-solver rows."
    ),
    "frozen_baseline_scope": "process_wall",
}

COLD_VS_REPEAT = {
    "cold": (
        "First solve of an instance by a solver in a run: fresh process, no "
        "reused basis, no warm OS page cache for the model file."
    ),
    "repeat": (
        "Any later solve of the same instance by the same solver in the same "
        "run: warm; reported separately and never averaged with the cold row."
    ),
    "frozen_baseline_reports": "cold",
}

DEFAULTS_STATUS = [
    {
        "name": "lp_qp_time_limit_s",
        "value": LP_TIME_LIMIT_S,
        "roadmap_target_s": 60,
        "harness_default": "run_full_compare --timeout default 60 s; run_full_benchmark "
                           "--timeout default 60 s for the netlib and qplib suites",
        "status": "kept",
        "note": "Both harnesses take the value from this module when --timeout is unset.",
    },
    {
        "name": "mip_time_limit_s",
        "value": MIP_TIME_LIMIT_S,
        "roadmap_target_s": 300,
        "harness_default": "run_full_benchmark --timeout default 300 s for the miplib, "
                           "mittelmann and cases suites; run_full_compare applies one "
                           "shared --timeout (default 60 s) to every class",
        "status": "deviation",
        "note": "run_full_compare cannot vary the cap per class without rewriting its "
                "shared report text, so MILP rows of a default compare run are LP-capped; "
                "pass --timeout 300 to apply the frozen MILP cap to that harness.",
    },
    {
        "name": "release_mip_time_limit_s",
        "value": RELEASE_MIP_TIME_LIMIT_S,
        "roadmap_target_s": 3600,
        "harness_default": "not used by the diagnostic harnesses",
        "status": "target_only",
        "note": "Release-campaign cap; recorded so a release run is not confused with the 300 s diagnostic cap.",
    },
    {
        "name": "min_repeats",
        "value": MIN_REPEATS,
        "roadmap_target_s": None,
        "harness_default": "run_full_compare and run_full_benchmark are single-shot; "
                           "run_compare --repeat defaults to 3",
        "status": "deviation",
        "note": "Roadmap asks for >=5 repeats per reported cell; the frozen baseline is single-shot, so its timings are cold single observations, not medians.",
    },
    {
        "name": "matched_threads",
        "value": MATCHED_THREADS,
        "roadmap_target_s": None,
        "harness_default": "--threads 1 on both harnesses",
        "status": "kept",
        "note": "Threads are matched across solvers so wall-clock rows are comparable.",
    },
    {
        "name": "timing_scope",
        "value": TIMING_SCOPE["frozen_baseline_scope"],
        "roadmap_target_s": None,
        "harness_default": "both harnesses measure the parent wall clock around the solver process",
        "status": "kept",
        "note": "Process-wall only; engine-only numbers stay in solver-native reports.",
    },
    {
        "name": "cold_vs_repeat",
        "value": COLD_VS_REPEAT["frozen_baseline_reports"],
        "roadmap_target_s": None,
        "harness_default": "first (and only) observation per instance/solver cell",
        "status": "kept",
        "note": "Single-shot runs are cold by construction; warm repeats are a follow-up.",
    },
]


def class_time_limit(problem_class: str) -> float:
    """Frozen per-solve cap for a problem class ('LP', 'QP', 'MILP')."""
    return CLASS_TIME_LIMITS.get(str(problem_class).upper(), MIP_TIME_LIMIT_S)


def suite_time_limit(suite: str) -> float:
    """Frozen per-solve cap for a benchmark suite name."""
    return class_time_limit(SUITE_CLASS.get(str(suite), "MILP"))


# Single frozen object: harness entry points pin this exact dict so a divergent
# copy of the definitions cannot be introduced silently.
TIMING = {
    "frozen_on": FROZEN_ON,
    "time_limits_s": {
        "lp": LP_TIME_LIMIT_S,
        "qp": QP_TIME_LIMIT_S,
        "mip": MIP_TIME_LIMIT_S,
        "release_mip": RELEASE_MIP_TIME_LIMIT_S,
    },
    "min_repeats": MIN_REPEATS,
    "matched_threads": MATCHED_THREADS,
    "class_time_limits_s": dict(CLASS_TIME_LIMITS),
    "suite_class": dict(SUITE_CLASS),
    "timing_scope": dict(TIMING_SCOPE),
    "cold_vs_repeat": dict(COLD_VS_REPEAT),
    "defaults_status": list(DEFAULTS_STATUS),
}


def as_dict() -> dict:
    """Machine-readable copy for the frozen manifests/evidence."""
    return dict(TIMING)
