#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.run_gpu_detect_hardware import find_solver_binary
from scripts.support.run_gpu_detect_hardware import _sh
from scripts.support.run_gpu_detect_hardware import detect_hardware
from scripts.support.run_gpu_detect_hardware import print_hardware
from scripts.support.run_gpu_run_configuration import run_configuration
from scripts.support.run_gpu_run_configuration import configuration_failures
from scripts.support.run_gpu_run_configuration import failed_record
from scripts.support.run_gpu_run_configuration import fmt_speedup
from scripts.support.run_gpu_main import main
from scripts.support.run_gpu_config import (
    Any, BENCHMARKS, DEFAULT_INSTANCES, Dict, List, Optional, RECORD_FIELDS, TABLE_WIDTH, _ENTRY_POINT, _EntryPath, argparse, csv, datetime, hashlib, json, math, os, platform, shutil, subprocess, sys, textwrap, time
)

if __name__ == "__main__":
    raise SystemExit(main())
