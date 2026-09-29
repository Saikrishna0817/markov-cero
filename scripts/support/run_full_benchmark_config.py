from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/run_full_benchmark.py')
import argparse
import csv
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path
REPO = Path(_ENTRY_POINT).resolve().parent.parent
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))
from .frozen_timing_config import LP_TIME_LIMIT_S, MIP_TIME_LIMIT_S
from .frozen_timing_config import QP_TIME_LIMIT_S, RELEASE_MIP_TIME_LIMIT_S
from .frozen_timing_config import MIN_REPEATS, MATCHED_THREADS, DEFAULTS_STATUS
from .frozen_timing_config import SUITE_CLASS, class_time_limit, suite_time_limit
# Frozen timing (backlog item 7): --timeout defaults to the suite cap
# (LP/QP 60 s, MILP 300 s) resolved from the frozen module above.
DEFAULT_TIMEOUT_S = LP_TIME_LIMIT_S

SUITES = {
    "netlib": {
        "dir": "data/netlib",
        "engine": "auto",
        "glob": "*.mps",
    },
    "miplib": {
        "dir": "data/miplib",
        "engine": "auto",
        "glob": "*.mps",
    },
    "mittelmann": {
        "dir": "data/mittelmann",
        "engine": "auto",
        "glob": "*.mps",
    },
    "qplib": {
        "dir": "data/qp",
        "engine": "auto",
        "glob": "QPLIB_*.mps",
    },
    "cases": {
        "dir": "data/cases",
        "engine": "auto",
        "glob": "*.mps",
    },
}
