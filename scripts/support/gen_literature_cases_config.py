from pathlib import Path as _EntryPath
_ENTRY_POINT = str(_EntryPath(__file__).resolve().parents[2] / 'scripts/generators/gen_literature_cases.py')
import argparse
import json
import os
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(_ENTRY_POINT))))
