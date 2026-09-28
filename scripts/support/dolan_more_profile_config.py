from __future__ import annotations
from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/dolan_more_profile.py')
import argparse
import csv
import math
import os
import sys
REPO = os.path.dirname(os.path.dirname(os.path.abspath(_ENTRY_POINT)))
DEFAULT_INPUT = os.path.join(REPO, "evidence", "comparison", "full_compare_results.csv")
DEFAULT_SVG = os.path.join(REPO, "evidence", "comparison", "dolan_more_runtime_profile.svg")
DEFAULT_CSV = os.path.join(REPO, "evidence", "comparison", "dolan_more_profile_data.csv")
SOLVER_ORDER = ["markov-cero", "HiGHS", "GLPK", "CBC", "SCIP"]
SOLVER_STYLE = {
    "markov-cero": ("#38bdf8", "none", "markov-cero (our binary)"),
    "HiGHS": ("#ef4444", "none", "HiGHS (highspy)"),
    "GLPK": ("#a78bfa", "3,2", "GLPK (glpsol)"),
    "CBC": ("#10b981", "4,2", "CBC (pulp)"),
    "SCIP": ("#f59e0b", "8,3", "SCIP (PySCIPOpt)"),
}
FALLBACK_COLORS = ["#a78bfa", "#f472b6", "#22d3ee", "#facc15"]
