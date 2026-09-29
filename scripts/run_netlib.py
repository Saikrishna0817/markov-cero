#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
# Backlog item 7: this harness entry point runs under the frozen timing
# definitions (LP/QP 60 s, MIP 300 s, release MIP 3600 s, >=5 repeats).
from scripts.support.frozen_timing_config import TIMING as FROZEN_TIMING
from scripts.support.run_netlib_run_instance import find_solver_binary
from scripts.support.run_netlib_run_instance import ensure_instance
from scripts.support.run_netlib_run_instance import run_instance
from scripts.support.run_netlib_main import main
from scripts.support.run_netlib_config import (
    Any, BASE_URL, Dict, EXTENDED_INSTANCES, List, NETLIB_BENCHMARKS, OFFLINE_INSTANCES, Optional, _ENTRY_POINT, _EntryPath, argparse, csv, gzip, json, os, subprocess, sys, time, urllib
)

if __name__ == "__main__":
    raise SystemExit(main())
