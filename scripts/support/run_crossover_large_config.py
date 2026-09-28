from __future__ import annotations
from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/run_crossover_large.py')
import argparse
import csv
import json
import math
import os
import random
import subprocess
import sys
import time
FIELDNAMES = ["instance", "backend", "nnz", "status", "verified", "total_ms"]
SPECS = [
    ("scale_100k", 500, 198, 101),    # nnz = 100,198
    ("scale_500k", 800, 622, 202),    # nnz = 499,822
    ("scale_1m", 1000, 997, 303),     # nnz = 999,997
    ("scale_2m", 1400, 1426, 404),    # nnz = 2,000,626
    ("scale_5m", 2000, 2497, 505),    # nnz = 5,000,497
]
MPS_LINE_BUDGET = 950_000  # CLI parser leaves maximum_lines at 1,000,000
DEFAULT_TARGET = "/tmp/opencode/crossover_instances"
DEFAULT_LOGS = "/tmp/opencode/crossover_logs"
OBJ_ROW = "COST"
