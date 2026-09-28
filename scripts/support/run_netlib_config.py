from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/run_netlib.py')
import argparse
import csv
import gzip
import json
import os
import subprocess
import sys
import time
import urllib.request
from typing import Dict, Any, List, Optional
NETLIB_BENCHMARKS = {
    "afiro": {
        "rows": 28,
        "cols": 32,
        "optimal": -464.753142857143,
        "description": "Michael Saunders, Stanford Systems Optimization Lab",
    },
    "adlittle": {
        "rows": 57,
        "cols": 97,
        "optimal": 225494.96316238,
        "description": "Arthur D. Little refinery model",
    },
    "sc50a": {
        "rows": 51,
        "cols": 48,
        "optimal": -64.575077058564,
        "description": "Staircase structure model A (50 stages)",
    },
    "sc50b": {
        "rows": 51,
        "cols": 48,
        "optimal": -70.000000000000,
        "description": "Staircase structure model B (50 stages)",
    },
    "sc105": {
        "rows": 106,
        "cols": 103,
        "optimal": -52.202061211707,
        "description": "Staircase structure model (105 stages)",
    },
    "sc205": {
        "rows": 206,
        "cols": 203,
        "optimal": -52.202061211707,
        "description": "Staircase structure model (205 stages)",
    },
    "share2b": {
        "rows": 97,
        "cols": 79,
        "optimal": -415.732240741418,
        "description": "Share model 2B",
    },
    "share1b": {
        "rows": 118,
        "cols": 225,
        "optimal": -76589.3185791855,
        "description": "Share model 1B",
    },
    "recipe": {
        "rows": 92,
        "cols": 180,
        "optimal": -266.616000000000,
        "description": "Food recipe formulation model",
    },
    "scagr7": {
        "rows": 130,
        "cols": 140,
        "optimal": -2331389.82433098,
        "description": "Agricultural multi-period model (7 periods)",
    },
    "beaconfd": {
        "rows": 174,
        "cols": 262,
        "optimal": 33592.4858072000,
        "description": "Beacon food distribution problem",
    },
    "scorpion": {
        "rows": 389,
        "cols": 358,
        "optimal": 1878.1248227381,
        "description": "Scorpion energy/flow model",
    },
}
BASE_URL = "https://raw.githubusercontent.com/coin-or-tools/Data-Netlib/master"
OFFLINE_INSTANCES = [
    "afiro", "adlittle", "sc50a", "sc50b", "sc105", "share2b", "recipe"
]
EXTENDED_INSTANCES = [
    "sc205", "share1b", "scagr7", "beaconfd", "scorpion"
]
