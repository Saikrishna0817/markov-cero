from __future__ import annotations
from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/download_mittelmann_suite.py')
import argparse
import bz2
import gzip
import hashlib
import json
import os
import pathlib
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from urllib.parse import urljoin
REPO = pathlib.Path(_ENTRY_POINT).resolve().parent.parent
DATA_DIR = REPO / "data" / "mittelmann"
MILP_PAGE = "https://plato.asu.edu/ftp/milp.html"
MILP_RES = "https://plato.asu.edu/ftp/milp_tables/12threads.res"
LPTESTSET = "https://plato.asu.edu/ftp/lptestset/"
LPTESTSET_DIRS = ("", "misc/", "pds/", "nug/", "fome/", "network/", "rail/", "fctp/")
MIPLIB_INSTANCE = "https://miplib.zib.de/WebData/instances/{name}.mps.gz"
USER_AGENT = "markov-cero-Dataset-Ingest"
MAX_COMPRESSED = 6 * 1024 * 1024
MAX_STORED = 15 * 1024 * 1024
REQUEST_TIMEOUT = 90
MAX_ATTEMPTS = 4
LP_TARGET = 10
MILP_TARGET = 4
LEGACY_SOURCES = {
    "bienst1": (
        MIPLIB_INSTANCE.format(name="bienst1"),
        "identical to the MIPLIB copy apart from trailing whitespace on the NAME line",
    ),
    "bienst2": (
        MIPLIB_INSTANCE.format(name="bienst2"),
        "identical to the MIPLIB copy apart from trailing whitespace on the NAME line",
    ),
    "markshare_5_0": (
        MIPLIB_INSTANCE.format(name="markshare_5_0"),
        "identical to the MIPLIB copy apart from the NAME line (MPSDATA vs markshare_5_0)",
    ),
    "mkc1": (
        MIPLIB_INSTANCE.format(name="mkc1"),
        "identical to the MIPLIB copy apart from trailing whitespace on the NAME line",
    ),
    "neos5": (
        MIPLIB_INSTANCE.format(name="neos5"),
        "byte-identical to the MIPLIB copy",
    ),
    "ran14x18_1": (
        "data/mittelmann/ran14x18_1.mps",
        "no upstream URL found (legacy MIPLIB-era instance); on-disk path recorded",
    ),
}
EMPS_SOURCE = "http://www.netlib.org/lp/data/emps.c"
SKIP_LP_TOKENS = {
    "probs", "scaled", "unscaled", "solved", "name", "instance", "testset",
    "notes", "note", "the", "this", "logfiles", "last",
}
