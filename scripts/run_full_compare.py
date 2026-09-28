#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.run_full_compare_run_highs import find_glpsol
from scripts.support.run_full_compare_run_highs import ensure_runtime_env
from scripts.support.run_full_compare_run_highs import mps_path
from scripts.support.run_full_compare_run_highs import rel_diff
from scripts.support.run_full_compare_run_highs import has_quadratic_objective
from scripts.support.run_full_compare_run_highs import prepare_mps_for_scip
from scripts.support.run_full_compare_run_highs import _markov_status
from scripts.support.run_full_compare_run_highs import run_markov
from scripts.support.run_full_compare_run_highs import run_highs
from scripts.support.run_full_compare_run_highs import _cbc_number
from scripts.support.run_full_compare_run_highs import parse_cbc_output
from scripts.support.run_full_compare_run_glpk import run_glpk
from scripts.support.run_full_compare_run_glpk import run_cbc_binary
from scripts.support.run_full_compare_run_glpk import run_cbc
from scripts.support.run_full_compare_run_glpk import run_scip
from scripts.support.run_full_compare_run_glpk import solve_task
from scripts.support.run_full_compare_run_glpk import _probe
from scripts.support.run_full_compare_annotate import probe_availability
from scripts.support.run_full_compare_run_glpk import pulp_version
from scripts.support.run_full_compare_annotate import load_reference_objectives
from scripts.support.run_full_compare_annotate import annotate
from scripts.support.run_full_compare_annotate import geomean
from scripts.support.run_full_compare_annotate import aggregates
from scripts.support.run_full_compare_annotate import write_results_csv
from scripts.support.run_full_compare_write_plan_comparison_exports import write_plan_comparison_exports
from scripts.support.run_full_compare_write_plan_comparison_exports import write_mittelmann_reference
from scripts.support.run_full_compare_write_plan_comparison_exports import parse_mittelmann_reference
from scripts.support.run_full_compare_write_plan_comparison_exports import fmt
from scripts.support.run_full_compare_build_report import build_report
from scripts.support.run_full_compare_main import main
from scripts.support.run_full_compare_config import (
    AGREE_TOL, CURATED, GLPK_NOTE, MPS_SECTIONS, OUT, REPO, SCRIPTS, SOLVER_ORDER, TABLES, ThreadPoolExecutor, _ENTRY_POINT, _EntryPath, argparse, csv, datetime, hashlib, importlib, json, math, os, re, shutil, subprocess, sys, tempfile, time, wait
)

if __name__ == "__main__":
    sys.exit(main())
