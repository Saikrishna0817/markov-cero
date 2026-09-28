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
REPO = os.path.dirname(os.path.dirname(os.path.abspath(_ENTRY_POINT)))
OUT = os.path.join(REPO, "evidence", "comparison")
TABLES = os.path.join(REPO, "data", "mittelmann_tables")
SCRIPTS = os.path.join(REPO, "scripts")
AGREE_TOL = 1e-4
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
