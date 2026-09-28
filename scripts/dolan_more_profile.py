#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.dolan_more_profile_generate_svg import load_results
from scripts.support.dolan_more_profile_generate_svg import performance_ratios
from scripts.support.dolan_more_profile_generate_svg import rho_value
from scripts.support.dolan_more_profile_generate_svg import log_grid
from scripts.support.dolan_more_profile_generate_svg import polyline_samples
from scripts.support.dolan_more_profile_generate_svg import write_profile_csv
from scripts.support.dolan_more_profile_generate_svg import generate_svg
from scripts.support.dolan_more_profile_main import main
from scripts.support.dolan_more_profile_config import (
    DEFAULT_CSV, DEFAULT_INPUT, DEFAULT_SVG, FALLBACK_COLORS, REPO, SOLVER_ORDER, SOLVER_STYLE, _ENTRY_POINT, _EntryPath, argparse, csv, math, os, sys
)

if __name__ == "__main__":
    sys.exit(main())
