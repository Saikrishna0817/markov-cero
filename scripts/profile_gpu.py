#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.profile_gpu_format_report_markdown import find_solver_binary
from scripts.support.profile_gpu_format_report_markdown import run_solver_timed
from scripts.support.profile_gpu_format_report_markdown import compute_profiling_metrics
from scripts.support.profile_gpu_format_report_markdown import format_report_markdown
from scripts.support.profile_gpu_main import main
from scripts.support.profile_gpu_config import (
    Any, Dict, List, Optional, _ENTRY_POINT, _EntryPath, argparse, json, math, os, shutil, subprocess, sys, time
)

if __name__ == "__main__":
    raise SystemExit(main())
