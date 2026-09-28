from __future__ import annotations
from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/import_qplib.py')
import argparse
import hashlib
import json
import pathlib
import sys
import zipfile
INF = float("inf")
