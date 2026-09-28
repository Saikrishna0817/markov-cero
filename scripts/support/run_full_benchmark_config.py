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
