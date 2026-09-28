#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.run_full_benchmark_run_one import qplib_convexity_gate
from scripts.support.run_full_benchmark_run_one import run_one
from scripts.support.run_full_benchmark_main import main
from scripts.support.run_full_benchmark_config import (
    Path, REPO, SUITES, _ENTRY_POINT, _EntryPath, argparse, csv, hashlib, json, subprocess, sys, time
)

if __name__ == "__main__":
    sys.exit(main())
