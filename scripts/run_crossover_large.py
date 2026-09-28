#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.run_crossover_large_solve_once import q10
from scripts.support.run_crossover_large_solve_once import fmt
from scripts.support.run_crossover_large_solve_once import varname
from scripts.support.run_crossover_large_solve_once import rowname
from scripts.support.run_crossover_large_solve_once import build_instance
from scripts.support.run_crossover_large_solve_once import nnz_of
from scripts.support.run_crossover_large_solve_once import estimated_mps_lines
from scripts.support.run_crossover_large_solve_once import write_mps
from scripts.support.run_crossover_large_solve_once import write_lp
from scripts.support.run_crossover_large_solve_once import generate_instance
from scripts.support.run_crossover_large_solve_once import probe_cuda
from scripts.support.run_crossover_large_solve_once import parse_json_line
from scripts.support.run_crossover_large_solve_once import solve_once
from scripts.support.run_crossover_large_main import main
from scripts.support.run_crossover_large_config import (
    DEFAULT_LOGS, DEFAULT_TARGET, FIELDNAMES, MPS_LINE_BUDGET, OBJ_ROW, SPECS, _ENTRY_POINT, _EntryPath, argparse, csv, json, math, os, random, subprocess, sys, time
)

if __name__ == "__main__":
    sys.exit(main())
