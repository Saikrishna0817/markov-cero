#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.download_mittelmann_suite_mps_stats import fetch
from scripts.support.download_mittelmann_suite_mps_stats import text
from scripts.support.download_mittelmann_suite_mps_stats import looks_like_mps
from scripts.support.download_mittelmann_suite_mps_stats import sha256_file
from scripts.support.download_mittelmann_suite_mps_stats import mps_stats
from scripts.support.download_mittelmann_suite_mps_stats import decompress
from scripts.support.download_mittelmann_suite_mps_stats import is_mpc
from scripts.support.download_mittelmann_suite_mps_stats import ensure_emps
from scripts.support.download_mittelmann_suite_mps_stats import format_note
from scripts.support.download_mittelmann_suite_mps_stats import expand
from scripts.support.download_mittelmann_suite_mps_stats import list_directory
from scripts.support.download_mittelmann_suite_mps_stats import strip_archive
from scripts.support.download_mittelmann_suite_mps_stats import discover_lp_files
from scripts.support.download_mittelmann_suite_mps_stats import discover_milp_names
from scripts.support.download_mittelmann_suite_main import existing_elsewhere
from scripts.support.download_mittelmann_suite_main import write_provenance
from scripts.support.download_mittelmann_suite_main import main
from scripts.support.download_mittelmann_suite_config import (
    DATA_DIR, EMPS_SOURCE, LEGACY_SOURCES, LPTESTSET, LPTESTSET_DIRS, LP_TARGET, MAX_ATTEMPTS, MAX_COMPRESSED, MAX_STORED, MILP_PAGE, MILP_RES, MILP_TARGET, MIPLIB_INSTANCE, REPO, REQUEST_TIMEOUT, SKIP_LP_TOKENS, USER_AGENT, _ENTRY_POINT, _EntryPath, argparse, bz2, gzip, hashlib, json, os, pathlib, random, re, shutil, subprocess, sys, tempfile, time, urljoin, urllib
)

if __name__ == "__main__":
    raise SystemExit(main())
