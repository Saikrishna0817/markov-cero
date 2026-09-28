#!/usr/bin/env python3
"""External comparison process only; never imported by the solver runtime."""
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.run_full_compare_run_highs import _run_highs_in_process

if __name__ == '__main__':
    result = _run_highs_in_process(sys.argv[1], float(sys.argv[2]), int(sys.argv[3]))
    print(json.dumps(result))
