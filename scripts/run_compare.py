#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.run_compare_run_markov_cero import repo_path
from scripts.support.run_compare_run_markov_cero import load_instance_list
from scripts.support.run_compare_run_markov_cero import sha256_file
from scripts.support.run_compare_run_markov_cero import find_solver
from scripts.support.run_compare_run_markov_cero import run_markov_cero
from scripts.support.run_compare_run_markov_cero import run_highs
from scripts.support.run_compare_run_markov_cero import geometric_mean_ratio
from scripts.support.run_compare_run_markov_cero import dolan_more_profile
from scripts.support.run_compare_run_markov_cero import write_profile_svg
from scripts.support.run_compare_main import main
from scripts.support.run_compare_config import (
    REPO_ROOT, _ENTRY_POINT, _EntryPath, argparse, csv, hashlib, json, math, os, random, statistics, subprocess, sys, time
)

if __name__ == "__main__":
    raise SystemExit(main())
