"""Matched process-wall timing for the optional external HiGHS test oracle."""
import json
from pathlib import Path
import subprocess
import sys
import time


def run_highs_process(mps, timeout, threads=1, python_executable=None):
    worker = Path(__file__).resolve().parents[1] / 'highs_worker.py'
    started = time.perf_counter()
    try:
        run = subprocess.run([python_executable or sys.executable, str(worker), str(mps), str(timeout), str(threads)],
                             capture_output=True, text=True, timeout=timeout + 10)
        result = json.loads(run.stdout) if run.returncode == 0 else {
            'status': 'OracleError', 'objective': '', 'certified': False}
    except subprocess.TimeoutExpired:
        result = {'status': 'TimeLimit', 'objective': '', 'certified': False}
    except (OSError, ValueError):
        result = {'status': 'OracleError', 'objective': '', 'certified': False}
    result['runtime_ms'] = (time.perf_counter() - started) * 1000
    result['timing_scope'] = 'process wall: startup, imports, read, solve, output'
    return result


def run_solver_process(solver, mps, timeout, threads=1):
    worker = Path(__file__).resolve().parents[1] / 'solver_oracle_worker.py'
    started = time.perf_counter()
    try:
        run = subprocess.run([sys.executable, str(worker), solver, str(mps), str(timeout), str(threads)],
                             capture_output=True, text=True, timeout=timeout + 10)
        result = json.loads(run.stdout) if run.returncode == 0 else {
            'status': 'OracleError', 'objective': '', 'certified': False}
    except subprocess.TimeoutExpired:
        result = {'status': 'TimeLimit', 'objective': '', 'certified': False}
    except (OSError, ValueError):
        result = {'status': 'OracleError', 'objective': '', 'certified': False}
    result['runtime_ms'] = (time.perf_counter() - started) * 1000
    result['timing_scope'] = 'process wall: startup, imports, read, solve, output'
    return result
