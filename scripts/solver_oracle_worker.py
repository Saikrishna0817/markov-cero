#!/usr/bin/env python3
"""External benchmark worker; imports and solver startup are inside the timing boundary."""
import contextlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.support.run_full_compare_run_glpk import run_glpk, run_cbc, run_scip

if __name__ == '__main__':
    function = {'GLPK': run_glpk, 'CBC': run_cbc, 'SCIP': run_scip}[sys.argv[1]]
    with contextlib.redirect_stdout(sys.stderr):
        result = function(sys.argv[2], float(sys.argv[3]), int(sys.argv[4]))
    print(json.dumps(result))
