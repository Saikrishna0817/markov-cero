#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.plot_crossover_print_ascii_summary import _to_float
from scripts.support.plot_crossover_print_ascii_summary import _to_int
from scripts.support.plot_crossover_print_ascii_summary import _is_solved
from scripts.support.plot_crossover_print_ascii_summary import _solved_ms
from scripts.support.plot_crossover_print_ascii_summary import parse_crossover_csv
from scripts.support.plot_crossover_print_ascii_summary import _format_ms
from scripts.support.plot_crossover_print_ascii_summary import _flag
from scripts.support.plot_crossover_print_ascii_summary import find_crossover
from scripts.support.plot_crossover_print_ascii_summary import print_ascii_summary
from scripts.support.plot_crossover_generate_svg import generate_svg
from scripts.support.plot_crossover_main import main
from scripts.support.plot_crossover_config import (
    Any, BACKEND_TO_SERIES, Dict, List, Optional, SERIES_ORDER, SERIES_STYLE, SOLVED_STATUS, _ENTRY_POINT, _EntryPath, argparse, csv, math, os, sys
)

if __name__ == "__main__":
    raise SystemExit(main())
