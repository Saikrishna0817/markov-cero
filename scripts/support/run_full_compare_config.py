from __future__ import annotations
from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/run_full_compare.py')
import argparse
import csv
import datetime
import hashlib
import importlib.util
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, wait
from .frozen_timing_config import LP_TIME_LIMIT_S, MATCHED_THREADS, MIN_REPEATS
from .frozen_timing_config import QP_TIME_LIMIT_S, RELEASE_MIP_TIME_LIMIT_S
from .frozen_timing_config import MIP_TIME_LIMIT_S, DEFAULTS_STATUS, TIMING_SCOPE
from .frozen_timing_config import class_time_limit as frozen_class_time_limit
REPO = os.path.dirname(os.path.dirname(os.path.abspath(_ENTRY_POINT)))
OUT = os.path.join(REPO, "evidence", "comparison")
TABLES = os.path.join(REPO, "data", "mittelmann_tables")
SCRIPTS = os.path.join(REPO, "scripts")
AGREE_TOL = 1e-4
# Frozen timing (backlog item 7): --timeout defaults to the class cap below,
# i.e. 60 s for LP/QP and 300 s for MILP, instead of one cap for every class.
DEFAULT_TIMEOUT_S = LP_TIME_LIMIT_S

CURATED = [
    ("netlib", "LP", "afiro"),
    ("netlib", "LP", "adlittle"),
    ("netlib", "LP", "sc50a"),
    ("netlib", "LP", "blend"),
    ("netlib", "LP", "lotfi"),
    ("netlib", "LP", "kb2"),
    ("netlib", "LP", "scorpion"),
    ("netlib", "LP", "share2b"),
    ("netlib", "LP", "beaconfd"),
    ("netlib", "LP", "recipe"),
    ("miplib", "MILP", "stein9"),
    ("miplib", "MILP", "stein15"),
    ("miplib", "MILP", "flugpl"),
    ("miplib", "MILP", "pk1"),
    ("miplib", "MILP", "swath1"),
    ("mittelmann", "MILP", "markshare_5_0"),
    ("mittelmann", "MILP", "bienst1"),
    ("mittelmann", "MILP", "neos5"),
    ("mittelmann", "MILP", "ran14x18_1"),
    ("qp", "QP", "QPLIB_0001"),
    ("qp", "QP", "QPLIB_0002"),
    ("qp", "QP", "QPLIB_0010"),
    ("qp", "QP", "QPLIB_0025"),
]
SOLVER_ORDER = ["markov-cero", "HiGHS", "GLPK", "CBC", "SCIP"]
GLPK_NOTE = "glpsol is absent from .venv/bin and system PATH"
MPS_SECTIONS = ("NAME", "OBJSENSE", "OBJNAME", "ROWS", "COLUMNS", "RHS", "RANGES",
                "BOUNDS", "QUADOBJ", "QMATRIX", "QCMATRIX", "SOS", "INDICATORS",
                "ENDATA")
