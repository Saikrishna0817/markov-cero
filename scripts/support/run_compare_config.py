from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/run_compare.py')
import argparse
import csv
import hashlib
import json
import math
import os
import random
import statistics
import subprocess
import sys
import time
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(_ENTRY_POINT)))
