#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.import_qplib_LabelReader import Reader
from scripts.support.import_qplib_LabelReader import split_records
from scripts.support.import_qplib_LabelReader import LabelReader
from scripts.support.import_qplib_parse_annotated import parse_annotated
from scripts.support.import_qplib_parse_annotated import parse_qplib
from scripts.support.import_qplib_parse_annotated import parse_positional
from scripts.support.import_qplib_write_mps import write_mps
from scripts.support.import_qplib_write_mps import objective_at
from scripts.support.import_qplib_write_mps import solution_value
from scripts.support.import_qplib_import_one import import_one
from scripts.support.import_qplib_import_one import main
from scripts.support.import_qplib_config import (
    INF, _ENTRY_POINT, _EntryPath, argparse, hashlib, json, pathlib, sys, zipfile
)

if __name__ == "__main__":
    raise SystemExit(main())
