#!/usr/bin/env python3
"""M6-37 (Gap 6): automated multi-solver comparison + Dolan-More profiles.

Curated instance set (23 problems, kept to ~10 minutes of wall clock):

  netlib LP (10)      afiro adlittle sc50a blend lotfi kb2 scorpion share2b
                      beaconfd recipe
  miplib MILP (5)     stein9 stein15 flugpl pk1 swath1
  data/mittelmann (4) markshare_5_0 bienst1 neos5 ran14x18_1   (all MILP;
                      markshare_5_0 is the 6th MILP of the curated set)
  QP (4)              QPLIB_0001 QPLIB_0002 QPLIB_0010 QPLIB_0025

Solvers (each optional, each reported gracefully as "unavailable" when the
binary/module cannot run on this host):

  markov-cero   build_gap/markov-cero-solve (CLI, --engine auto)
  HiGHS         highspy, HiGHS Python API (readModel/run)
  CBC           pulp (bundled CBC binary), LpProblem.fromMPS + PULP_CBC_CMD
  SCIP          PySCIPOpt (readProblem/optimize)
  GLPK          glpsol subprocess (project-local .venv/bin/glpsol or system PATH)

Outputs (evidence/comparison/):
  full_compare_results.csv         instance,solver,status,objective,runtime_ms,verified
  full_comparison_report.md        availability + aggregates + Mittelmann reference
  mittelmann_reference.md          published reference (regenerated, extended)
  dolan_more_profile_data.csv      sampled (tau, solver, rho) points
  dolan_more_runtime_profile.svg   exact Dolan-More profile (scripts/dolan_more_profile.py)

Preserved untouched: netlib_comparison.csv, miplib_comparison.csv (W9 legacy,
"add, do not break").

Methodology: one time cap for every cell (default 60 s), the available solvers of an
instance run concurrently on this multi-core host so wall clock stays within
the budget, failures/timeouts are recorded and never dropped, and an objective
is "verified" only when it agrees (relative tolerance 1e-4) with an independent
reference: the provenance reference_objective when present, otherwise the
certified markov-cero objective.
"""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import importlib.util
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, wait

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "evidence", "comparison")
TABLES = os.path.join(REPO, "data", "mittelmann_tables")
SCRIPTS = os.path.join(REPO, "scripts")

AGREE_TOL = 1e-4

CURATED = [
    ("netlib", "LP", "afiro"),
    ("netlib", "LP", "adlittle"),
    ("netlib", "LP", "sc50a"),
    ("netlib", "LP", "blend"),
    ("netlib", "LP", "lotfi"),
    ("netlib", "LP", "kb2"),
    ("netlib", "LP", "scorpion"),
    ("netlib", "LP", "share2b"),
    ("netlib", "LP", "beaconfd"),
    ("netlib", "LP", "recipe"),
    ("miplib", "MILP", "stein9"),
    ("miplib", "MILP", "stein15"),
    ("miplib", "MILP", "flugpl"),
    ("miplib", "MILP", "pk1"),
    ("miplib", "MILP", "swath1"),
    ("mittelmann", "MILP", "markshare_5_0"),
    ("mittelmann", "MILP", "bienst1"),
    ("mittelmann", "MILP", "neos5"),
    ("mittelmann", "MILP", "ran14x18_1"),
    ("qp", "QP", "QPLIB_0001"),
    ("qp", "QP", "QPLIB_0002"),
    ("qp", "QP", "QPLIB_0010"),
    ("qp", "QP", "QPLIB_0025"),
]

SOLVER_ORDER = ["markov-cero", "HiGHS", "GLPK", "CBC", "SCIP"]

GLPK_NOTE = "glpsol is absent from .venv/bin and system PATH"


def find_glpsol() -> str | None:
    local = os.path.join(REPO, ".venv", "bin", "glpsol")
    if os.path.isfile(local) and os.access(local, os.X_OK):
        return local
    return shutil.which("glpsol")


def ensure_runtime_env() -> list:
    """Re-exec under .compare-venv when solver modules are missing; return names."""
    wanted = ["highspy", "pulp", "pyscipopt"]
    missing = [m for m in wanted if importlib.util.find_spec(m) is None]
    if missing and not os.environ.get("MARKOV_COMPARE_REEXEC"):
        venv_py = os.path.join(REPO, ".compare-venv", "bin", "python")
        # Virtualenv interpreters commonly symlink to the same system binary,
        # so compare prefixes rather than real executable paths.
        venv_root = os.path.dirname(os.path.dirname(venv_py))
        if (os.path.isfile(venv_py)
                and os.path.realpath(sys.prefix) != os.path.realpath(venv_root)):
            os.environ["MARKOV_COMPARE_REEXEC"] = "1"
            os.execv(venv_py, [venv_py, os.path.abspath(__file__)] + sys.argv[1:])
            return []
    return missing


def mps_path(suite: str, name: str) -> str:
    return os.path.join(REPO, "data", suite, f"{name}.mps")


def rel_diff(a: float, b: float) -> float:
    return abs(a - b) / max(1.0, abs(a), abs(b))


def has_quadratic_objective(mps: str) -> bool:
    try:
        with open(mps) as f:
            for line in f:
                token = line.strip().split()
                if token and token[0] in ("QUADOBJ", "QMATRIX", "QCMATRIX"):
                    return True
    except OSError:
        return False
    return False


MPS_SECTIONS = ("NAME", "OBJSENSE", "OBJNAME", "ROWS", "COLUMNS", "RHS", "RANGES",
                "BOUNDS", "QUADOBJ", "QMATRIX", "QCMATRIX", "SOS", "INDICATORS",
                "ENDATA")


def prepare_mps_for_scip(mps: str) -> str:
    """Reformat a quadratic MPS file for SCIP's reader.

    SCIP 10 refuses QUADOBJ before BOUNDS and refuses BOUNDS without a preceding
    RHS section.  Sections are reordered (math is unchanged) and an empty RHS
    section is injected when the source has none.  Returns a temp path.
    """
    header, sections, order = [], {}, []
    current = None
    with open(mps) as f:
        for line in f:
            token = line.strip().split()
            key = token[0] if token and token[0] in MPS_SECTIONS else None
            if key:
                current = key
                sections.setdefault(key, [])
                order.append(key)
            if current is None:
                header.append(line)
            else:
                sections[current].append(line)

    out = list(header)
    for key in ("NAME", "OBJSENSE", "OBJNAME", "ROWS", "COLUMNS"):
        out += sections.get(key, [])
    if "RHS" in sections:
        out += sections["RHS"]
    else:
        out += ["RHS\n", "    _RHS_  OBJ  0\n"]
    out += sections.get("RANGES", [])
    out += sections.get("BOUNDS", [])
    out += sections.get("QUADOBJ", [])
    out += sections.get("QMATRIX", [])
    out += sections.get("QCMATRIX", [])
    for key in order:
        if key in ("SOS", "INDICATORS"):
            out += sections[key]
    out += ["ENDATA\n"]

    handle, path = tempfile.mkstemp(prefix="markov_scip_", suffix=".mps")
    with os.fdopen(handle, "w") as f:
        f.writelines(out)
    return path


def _markov_status(raw: str) -> str:
    mapping = {
        "ResourceLimit": "TimeLimit",
        "NodeLimit": "TimeLimit",
        "IterationLimit": "IterationLimit",
        "Optimal": "Optimal",
        "Infeasible": "Infeasible",
        "Unbounded": "Unbounded",
        "NumericalFailure": "NumericalFailure",
    }
    return mapping.get(raw, raw or "Unknown")


def run_markov(binary: str, mps: str, timeout: float, threads: int = 1) -> dict:
    started = time.perf_counter()
    try:
        proc = subprocess.run(
            [binary, mps, "--time-limit", str(int(timeout)),
             "--threads", str(int(threads))],
            capture_output=True, text=True, timeout=timeout + 20)
    except subprocess.TimeoutExpired:
        return {"status": "TimeLimit", "objective": "", "certified": False,
                "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1),
                "version": "", "engine": ""}
    runtime_ms = round((time.perf_counter() - started) * 1000.0, 1)
    payload = None
    for line in reversed(proc.stdout.splitlines()):
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            try:
                payload = json.loads(line)
            except json.JSONDecodeError:
                payload = None
            break
    if payload is None:
        return {"status": f"Error(exit {proc.returncode})", "objective": "",
                "certified": False, "runtime_ms": runtime_ms,
                "version": "", "engine": ""}
    status = _markov_status(str(payload.get("status", "")))
    objective = ""
    if status == "Optimal" and payload.get("objective") not in (None, ""):
        objective = payload.get("objective")
    return {"status": status, "objective": objective,
            "certified": bool(payload.get("verified", False)),
            "runtime_ms": runtime_ms,
            "version": str(payload.get("version", "")),
            "engine": str(payload.get("engine", ""))}


def run_highs(mps: str, timeout: float, threads: int = 1) -> dict:
    import highspy
    highs = highspy.Highs()
    highs.setOptionValue("output_flag", False)
    highs.setOptionValue("time_limit", float(timeout))
    try:
        highs.setOptionValue("threads", int(threads))
    except Exception:
        pass
    started = time.perf_counter()
    try:
        if highs.readModel(mps) != highspy.HighsStatus.kOk:
            return {"status": "Error(readModel)", "objective": "", "certified": False,
                    "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1)}
        highs.run()
    except Exception as exc:
        return {"status": f"Error({type(exc).__name__})", "objective": "",
                "certified": False,
                "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1)}
    runtime_ms = round((time.perf_counter() - started) * 1000.0, 1)
    raw = highspy.HighsModelStatus(highs.getModelStatus()).name
    info = highs.getInfo()
    solution = highs.getSolution()
    value_valid = bool(solution is not None and solution.value_valid)
    objective = ""
    if raw == "kOptimal":
        status = "Optimal"
        objective = info.objective_function_value
    elif raw == "kTimeLimit":
        if value_valid:
            status = "Feasible"
            objective = info.objective_function_value
        else:
            status = "TimeLimit"
    elif raw == "kInfeasible":
        status = "Infeasible"
    elif raw == "kUnbounded":
        status = "Unbounded"
    elif raw == "kUnboundedOrInfeasible":
        status = "UnboundedOrInfeasible"
    else:
        status = raw[1:] if raw.startswith("k") else raw
    return {"status": status, "objective": objective, "certified": False,
            "runtime_ms": runtime_ms}


def _cbc_number(out: str, patterns) -> float:
    for pattern in patterns:
        match = re.search(pattern, out, re.M)
        if match:
            text = match.group(1)
            if text.lower() in ("inf", "-inf", "nan", "+inf"):
                continue
            try:
                return float(text)
            except ValueError:
                continue
    return None


def parse_cbc_output(out: str, runtime_ms: float, timeout: float) -> dict:
    objective = _cbc_number(out, (
        r"^Optimal objective\s+(\S+)",
        r"^Objective value:\s+(\S+)",
        r"^Optimal - objective value\s+(\S+)",
    ))
    if ("Result - Optimal solution found" in out
            or re.search(r"^Optimal objective\s+\S", out, re.M)
            or "Optimal - objective value" in out):
        status = "Optimal" if objective is not None else "NotSolved"
    elif "Result - Stopped on time limit" in out:
        status = "Feasible" if objective is not None else "TimeLimit"
    elif "Result - No feasible solution found" in out:
        status = "TimeLimit" if runtime_ms >= 0.9 * timeout * 1000.0 else "NoSolution"
    elif "Problem is infeasible" in out or "Result - Problem proven infeasible" in out:
        status, objective = "Infeasible", None
    elif "is unbounded" in out or "Result - Problem proven unbounded" in out:
        status, objective = "Unbounded", None
    elif runtime_ms >= 0.9 * timeout * 1000.0:
        status = "TimeLimit" if objective is None else "Feasible"
    else:
        status = "NotSolved"
    if status not in ("Optimal", "Feasible"):
        objective = None
    return {"status": status, "objective": "" if objective is None else objective,
            "certified": False, "runtime_ms": runtime_ms}


def run_glpk(mps: str, timeout: float, threads: int = 1) -> dict:
    if has_quadratic_objective(mps):
        return {"status": "Unsupported", "objective": "", "certified": False,
                "runtime_ms": 0.0, "detail": "GLPK does not solve quadratic objectives"}
    binary = find_glpsol()
    if not binary:
        return {"status": "Unavailable", "objective": "", "certified": False,
                "runtime_ms": 0.0}
    started = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="markov_glpk_") as temp_dir:
        solution_path = os.path.join(temp_dir, "solution.txt")
        try:
            proc = subprocess.run(
                [binary, "--mps", mps, "--tmlim", str(max(1, math.ceil(timeout))),
                 "--output", solution_path],
                capture_output=True, text=True, timeout=timeout + 20)
        except subprocess.TimeoutExpired:
            return {"status": "TimeLimit", "objective": "", "certified": False,
                    "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1)}
        runtime_ms = round((time.perf_counter() - started) * 1000.0, 1)
        solution = ""
        if os.path.isfile(solution_path):
            with open(solution_path) as f:
                solution = f.read()
    status_match = re.search(r"^Status:\s+(.+)$", solution, re.M)
    objective_match = re.search(r"^Objective:\s+\S+\s*=\s*([+\-0-9.eE]+)",
                                solution, re.M)
    raw = status_match.group(1).strip().upper() if status_match else ""
    objective = float(objective_match.group(1)) if objective_match else ""
    if raw in ("OPTIMAL", "INTEGER OPTIMAL"):
        status = "Optimal" if objective != "" else "NotSolved"
    elif raw in ("FEASIBLE", "INTEGER NON-OPTIMAL") and objective != "":
        status = "Feasible"
    elif raw in ("INFEASIBLE", "INTEGER EMPTY"):
        status, objective = "Infeasible", ""
    elif raw == "UNBOUNDED":
        status, objective = "Unbounded", ""
    elif "TIME LIMIT EXCEEDED" in proc.stdout:
        status, objective = "TimeLimit", ""
    else:
        status, objective = f"Error(exit {proc.returncode})", ""
    return {"status": status, "objective": objective, "certified": False,
            "runtime_ms": runtime_ms, "detail": f"glpsol status: {raw or 'missing'}"}


def run_cbc_binary(mps: str, timeout: float, started: float, detail: str,
                   threads: int = 1) -> dict:
    import pulp
    cbc = getattr(pulp.PULP_CBC_CMD(msg=0), "path", None)
    if not cbc or not os.path.isfile(cbc):
        return {"status": "Unavailable", "objective": "", "certified": False,
                "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1),
                "detail": detail + "; bundled CBC binary not found"}
    try:
        proc = subprocess.run(
            [cbc, mps, "-threads", str(int(threads)), "-sec", str(timeout),
             "-timeMode", "elapsed", "-solve"],
            capture_output=True, text=True, timeout=timeout + 30)
    except subprocess.TimeoutExpired:
        return {"status": "TimeLimit", "objective": "", "certified": False,
                "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1),
                "detail": detail + "; CBC subprocess exceeded the cap"}
    runtime_ms = round((time.perf_counter() - started) * 1000.0, 1)
    result = parse_cbc_output(proc.stdout + "\n" + proc.stderr, runtime_ms, timeout)
    result["detail"] = detail
    return result


def run_cbc(mps: str, timeout: float, threads: int = 1) -> dict:
    if has_quadratic_objective(mps):
        return {"status": "Unsupported", "objective": "", "certified": False,
                "runtime_ms": 0.0,
                "detail": "CBC solves LP/MIP only; MPS quadratic objective not supported"}
    import pulp
    started = time.perf_counter()
    try:
        _, problem = pulp.LpProblem.fromMPS(mps)
    except Exception as exc:
        return run_cbc_binary(
            mps, timeout, started,
            detail=(f"pulp MPS reader failed ({type(exc).__name__}: {exc}); fell back "
                    f"to the CBC binary bundled with pulp"), threads=threads)
    try:
        problem.solve(pulp.PULP_CBC_CMD(msg=0, timeLimit=float(timeout),
                                        threads=int(threads)))
    except Exception as exc:
        return {"status": f"Error({type(exc).__name__})", "objective": "",
                "certified": False,
                "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1),
                "detail": str(exc)}
    runtime_ms = round((time.perf_counter() - started) * 1000.0, 1)
    status_code = int(problem.status)
    sol_status = int(getattr(problem, "sol_status", -99))
    objective = ""
    try:
        objective = problem.objective.value()
    except Exception:
        objective = None
    if sol_status == 1:
        status = "Optimal"
    elif sol_status == 2:
        status = "Feasible"
    elif status_code == -1:
        status = "Infeasible"
    elif status_code == -2:
        status = "Unbounded"
    elif objective is None:
        status = "NoSolution"
    elif runtime_ms >= 0.9 * timeout * 1000.0:
        status = "TimeLimit"
    else:
        status = "NotSolved"
    if status not in ("Optimal", "Feasible"):
        objective = ""
    return {"status": status, "objective": objective, "certified": False,
            "runtime_ms": runtime_ms}


def run_scip(mps: str, timeout: float, threads: int = 1) -> dict:
    from pyscipopt import Model
    temp = None
    path = mps
    if has_quadratic_objective(mps):
        temp = prepare_mps_for_scip(mps)
        path = temp
    model = Model()
    model.setParam("display/verblevel", 0)
    model.setParam("limits/time", float(timeout))
    try:
        model.setParam("parallel/maxnthreads", int(threads))
    except Exception:
        pass
    started = time.perf_counter()
    try:
        model.readProblem(path)
        model.optimize()
    except Exception as exc:
        if temp and os.path.isfile(temp):
            os.unlink(temp)
        return {"status": f"Error({type(exc).__name__})", "objective": "",
                "certified": False,
                "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1)}
    runtime_ms = round((time.perf_counter() - started) * 1000.0, 1)
    if temp and os.path.isfile(temp):
        os.unlink(temp)
    state = model.getStatus()
    objective = ""
    if state == "optimal":
        status = "Optimal"
        objective = model.getObjVal()
    elif state == "timelimit":
        if model.getNSols() > 0:
            status = "Feasible"
            objective = model.getObjVal()
        else:
            status = "TimeLimit"
    elif state == "infeasible":
        status = "Infeasible"
    elif state == "unbounded":
        status = "Unbounded"
    elif state == "gaplimit":
        status = "Feasible" if model.getNSols() > 0 else "TimeLimit"
        if model.getNSols() > 0:
            objective = model.getObjVal()
    elif state == "userinterrupt":
        status = "TimeLimit" if model.getNSols() > 0 else "NoSolution"
        if model.getNSols() > 0:
            status = "Feasible"
            objective = model.getObjVal()
    else:
        status = str(state)
    return {"status": status, "objective": objective, "certified": False,
            "runtime_ms": runtime_ms}


def solve_task(payload: dict) -> dict:
    solver = payload["solver"]
    started = time.perf_counter()
    try:
        if solver == "markov-cero":
            result = run_markov(payload["binary"], payload["mps"], payload["timeout"],
                                payload["threads"])
        elif solver == "HiGHS":
            result = run_highs(payload["mps"], payload["timeout"], payload["threads"])
        elif solver == "GLPK":
            result = run_glpk(payload["mps"], payload["timeout"], payload["threads"])
        elif solver == "CBC":
            result = run_cbc(payload["mps"], payload["timeout"], payload["threads"])
        elif solver == "SCIP":
            result = run_scip(payload["mps"], payload["timeout"], payload["threads"])
        else:
            result = {"status": "Unavailable", "objective": "", "certified": False,
                      "runtime_ms": 0.0}
    except Exception as exc:
        result = {"status": f"Error({type(exc).__name__})", "objective": "",
                  "certified": False,
                  "runtime_ms": round((time.perf_counter() - started) * 1000.0, 1),
                  "detail": str(exc)}
    return {"instance": payload["instance"], "solver": solver, **result}


def _probe(cmd, timeout=30):
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                              cwd=REPO)
        return proc.returncode, (proc.stdout + proc.stderr).strip()
    except Exception as exc:
        return -1, f"probe failed: {exc}"


def probe_availability(binary: str) -> dict:
    probes = {}

    def add(name, available, version, how, notes=""):
        probes[name] = {"available": bool(available), "version": version or "unknown",
                        "how": how, "notes": notes}

    if binary and os.path.isfile(binary) and os.access(binary, os.X_OK):
        rc, _ = _probe([binary, "--help"])
        add("markov-cero", rc == 0, "", "binary `--help` probe",
            "engine selection: auto; CLI solver")
    else:
        add("markov-cero", False, "", "binary probe",
            f"binary not found or not executable: {binary}")

    rc, out = _probe([sys.executable, "-c",
                      "import highspy; print(highspy.Highs().version())"])
    add("HiGHS", rc == 0, out if rc == 0 else "", "import highspy (in-process API)",
        "" if rc == 0 else out[-200:])

    rc, out = _probe([sys.executable, "-c",
                      "import pulp; from pulp import PULP_CBC_CMD; print(PULP_CBC_CMD(msg=0).path)"])
    cbc_notes = ""
    cbc_version = "unknown"
    if rc == 0 and out and os.path.isfile(out.splitlines()[-1]):
        cbc_path = out.splitlines()[-1]
        brc, banner = _probe([cbc_path], timeout=20)
        for line in banner.splitlines():
            if line.strip().startswith("Version:"):
                cbc_version = line.split(":", 1)[1].strip()
        add("CBC", True, cbc_version, "pulp PULP_CBC_CMD (bundled CBC binary)",
            f"pulp {pulp_version()} + CBC {cbc_version}")
    else:
        add("CBC", False, "", "pulp PULP_CBC_CMD",
            (out or "pulp not importable")[-200:])

    rc, out = _probe([sys.executable, "-c",
                      "import pyscipopt; from pyscipopt import Model; m=Model();"
                      "print('%s (PySCIPOpt %s)' % ("
                      "'.'.join(str(x) for x in (m.getMajorVersion(), m.getMinorVersion(),"
                      "m.getTechVersion())), pyscipopt.__version__))"])
    add("SCIP", rc == 0, out if rc == 0 else "", "import pyscipopt (in-process API)",
        "" if rc == 0 else out[-200:])

    glpsol = find_glpsol()
    if glpsol:
        rc, out = _probe([glpsol, "--version"])
        version = out.splitlines()[0].strip() if rc == 0 and out else "unknown"
        add("GLPK", rc == 0, version, glpsol,
            "external glpsol process; GLPK uses one thread")
    else:
        add("GLPK", False, "", "project-local .venv/bin or PATH", GLPK_NOTE)
    return probes


def pulp_version() -> str:
    try:
        import pulp
        return getattr(pulp, "__version__", "unknown")
    except Exception:
        return "unavailable"


def load_reference_objectives(instances) -> dict:
    refs = {}
    for suite, _, name in instances:
        path = os.path.join(REPO, "data", suite, f"{name}.provenance.json")
        if not os.path.isfile(path):
            continue
        try:
            with open(path) as f:
                payload = json.load(f)
        except (OSError, json.JSONDecodeError):
            continue
        value = payload.get("reference_objective")
        if value is not None:
            refs[name] = float(value)
    return refs


def annotate(rows, refs, instances):
    markov_optimal = {r["instance"]: r["objective"]
                      for r in rows
                      if r["solver"] == "markov-cero" and r["status"] == "Optimal"
                      and r["objective"] != ""}
    disagreements = []
    for row in rows:
        inst = row["instance"]
        if row["status"] != "Optimal" or row["objective"] == "":
            row["verified"] = False
            continue
        obj = float(row["objective"])
        if row["solver"] == "markov-cero":
            if inst in refs:
                row["verified"] = rel_diff(obj, refs[inst]) <= AGREE_TOL
            else:
                row["verified"] = bool(row.get("certified", False))
        else:
            reference = refs.get(inst, markov_optimal.get(inst))
            row["verified"] = (reference is not None
                               and rel_diff(obj, reference) <= AGREE_TOL)

    by_instance = {}
    for row in rows:
        by_instance.setdefault(row["instance"], []).append(row)
    for inst, group in by_instance.items():
        optimal = [r for r in group if r["status"] == "Optimal" and r["objective"] != ""]
        if inst in refs:
            reference, source = refs[inst], "provenance reference_objective"
            for row in optimal:
                rel = rel_diff(float(row["objective"]), reference)
                if rel > AGREE_TOL:
                    disagreements.append({
                        "instance": inst, "solver": row["solver"], "against": source,
                        "solver_objective": row["objective"],
                        "reference_objective": reference, "rel_diff": rel})
        elif inst in markov_optimal:
            reference = markov_optimal[inst]
            for row in optimal:
                if row["solver"] == "markov-cero":
                    continue
                rel = rel_diff(float(row["objective"]), reference)
                if rel > AGREE_TOL:
                    disagreements.append({
                        "instance": inst, "solver": row["solver"],
                        "against": "markov-cero certified objective",
                        "solver_objective": row["objective"],
                        "reference_objective": reference, "rel_diff": rel})
        else:
            for i in range(len(optimal)):
                for j in range(i + 1, len(optimal)):
                    a, b = optimal[i], optimal[j]
                    rel = rel_diff(float(a["objective"]), float(b["objective"]))
                    if rel > AGREE_TOL:
                        disagreements.append({
                            "instance": inst,
                            "solver": f"{a['solver']} vs {b['solver']}",
                            "against": "mutual (no independent reference)",
                            "solver_objective": a["objective"],
                            "reference_objective": b["objective"], "rel_diff": rel})
    return markov_optimal, disagreements


def geomean(values):
    values = [v for v in values if v is not None and v > 0 and math.isfinite(v)]
    if not values:
        return float("nan")
    return math.exp(sum(math.log(v) for v in values) / len(values))


def aggregates(rows, probes):
    out = []
    extra = sorted({r["solver"] for r in rows} - set(SOLVER_ORDER))
    for solver in SOLVER_ORDER + extra:
        if not probes.get(solver, {}).get("available", False) and \
                not any(r["solver"] == solver for r in rows):
            continue
        group = [r for r in rows if r["solver"] == solver]
        if not group:
            continue
        optimal = [r for r in group if r["status"] == "Optimal"]
        feasible = [r for r in group if r["status"] == "Feasible"]
        unsupported = [r for r in group if r["status"] == "Unsupported"]
        other = [r for r in group if r["status"] not in
                 ("Optimal", "Feasible", "Unsupported")]
        verified = [r for r in group if r.get("verified")]
        out.append({"solver": solver, "rows": len(group), "optimal": len(optimal),
                    "feasible": len(feasible), "unsupported": len(unsupported),
                    "other": len(other), "verified": len(verified)})
    return out


def write_results_csv(path, rows):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    fields = ["instance", "solver", "status", "objective", "runtime_ms", "verified",
              "markov_cero_sha256"]
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow({k: row.get(k, "") for k in fields})
    return path


def write_plan_comparison_exports(out_dir, rows, instances, refs):
    """Write the five W9 suite tables and separate LP/MILP profiles.

    Published Gurobi rows are not fabricated here: the local Mittelmann source
    table records heterogeneous published runtime fields, so that ratio stays
    blank until an exact solver/instance/time match can be established.
    """
    instance_meta = {name: (suite, cls) for suite, cls, name in instances}
    timings = {(r["instance"], r["solver"]): r for r in rows}
    groups = {
        "netlib_lp_comparison.csv": lambda s, c: s == "netlib" and c == "LP",
        "miplib_comparison.csv": lambda s, c: s == "miplib" and c == "MILP",
        "mittelmann_lp_comparison.csv": lambda s, c: s == "mittelmann" and c == "LP",
        "mittelmann_milp_comparison.csv": lambda s, c: s == "mittelmann" and c == "MILP",
        "qplib_comparison.csv": lambda s, c: s == "qp" and c == "QP",
    }
    fields = ["solver", "instance", "status", "objective", "obj_error_vs_best",
              "time_ms", "ratio_vs_highs", "ratio_vs_gurobi", "pass"]
    os.makedirs(out_dir, exist_ok=True)
    for filename, include in groups.items():
        path = os.path.join(out_dir, filename)
        with open(path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            for row in rows:
                suite, cls = instance_meta.get(row["instance"], ("", ""))
                if not include(suite, cls):
                    continue
                inst = row["instance"]
                obj = row.get("objective", "")
                known = refs.get(inst)
                if known is None:
                    mc = timings.get((inst, "markov-cero"), {})
                    if mc.get("status") == "Optimal" and mc.get("objective") != "":
                        known = float(mc["objective"])
                err = ""
                if obj not in (None, "") and known is not None:
                    err = rel_diff(float(obj), float(known))
                highs = timings.get((inst, "HiGHS"), {})
                ratio = ""
                if (row.get("status") == "Optimal" and highs.get("status") == "Optimal"
                        and float(highs.get("runtime_ms", 0.0)) > 0):
                    ratio = float(row["runtime_ms"]) / float(highs["runtime_ms"])
                writer.writerow({
                    "solver": row["solver"], "instance": inst,
                    "status": row.get("status", ""), "objective": obj,
                    "obj_error_vs_best": err, "time_ms": row.get("runtime_ms", ""),
                    "ratio_vs_highs": ratio, "ratio_vs_gurobi": "",
                    "pass": bool(row.get("status") == "Optimal" and row.get("verified")),
                })

    # Use the same profile implementation and denominator rules as the overall
    # profile; only the problem set changes by category.
    for cls, instances_in_class, suffix in (
        ("LP", {name for _, kind, name in instances if kind == "LP"}, "lp"),
        ("MILP", {name for _, kind, name in instances if kind == "MILP"}, "milp"),
    ):
        filtered = [r for r in rows if r["instance"] in instances_in_class]
        if not filtered:
            continue
        fd, temp_csv = tempfile.mkstemp(prefix=f"markov-{suffix}-", suffix=".csv")
        os.close(fd)
        try:
            write_results_csv(temp_csv, filtered)
            subprocess.run(
                [sys.executable, os.path.join(SCRIPTS, "dolan_more_profile.py"),
                 "--input", temp_csv,
                 "--svg", os.path.join(out_dir, f"dolan_more_{suffix}.svg"),
                 "--csv", os.path.join(out_dir, f"dolan_more_{suffix}_profile_data.csv")],
                check=True, capture_output=True, text=True, cwd=REPO)
        finally:
            os.unlink(temp_csv)
    return groups


def write_mittelmann_reference(instances) -> str:
    lines = ["# Mittelmann published-table reference (D-09)", ""]
    res_csv = os.path.join(TABLES, "milp_12threads.csv")
    published = {}
    if os.path.isfile(res_csv):
        with open(res_csv) as f:
            table_rows = list(csv.DictReader(f))
        solvers = [c for c in table_rows[0] if c not in ("instance", "url", "accessed")]
        lines += [f"Source: {table_rows[0]['url']} (accessed {table_rows[0]['accessed']})",
                  f"Instances: {len(table_rows)} (MIPLIB2017 benchmark, preprocessed)",
                  "",
                  "| solver | solved/total |",
                  "|---|---|"]
        solved = {s: sum(1 for r in table_rows
                         if r[s].strip().lower() not in ("timeout", "failed", ""))
                  for s in solvers}
        for s in solvers:
            lines.append(f"| {s} | {solved[s]}/{len(table_rows)} |")
        lines += ["", "markov-cero does not appear in these published tables; "
                      "the local open-source comparisons (results CSVs above) are "
                      "measured baselines. Published numbers provide the "
                      "commercial-solver context without licenses."]
        for row in table_rows:
            published[row["instance"].strip()] = row
    else:
        lines.append("No scraped tables found; run scripts/scrape_mittelmann.py.")

    lines += ["", "## Per-instance published entries (curated data/mittelmann set, M6-37)",
              "",
              "Instance names are matched exactly against "
              "`data/mittelmann_tables/milp_12threads.csv` after stripping the "
              "published `p_` prefix.  The published tables record **wall-clock "
              "times only** - they carry no objective values, so every objective "
              "entry below is marked unverified unless a provenance file supplies "
              "a `reference_objective`.", "",
              "| instance | published entry | published times (s) | reference objective | verified |",
              "|---|---|---|---|---|"]
    for suite, _, name in instances:
        if suite != "mittelmann":
            continue
        entry = published.get(f"p_{name}")
        ref_objective = None
        prov = os.path.join(REPO, "data", suite, f"{name}.provenance.json")
        if os.path.isfile(prov):
            try:
                with open(prov) as f:
                    ref_objective = json.load(f).get("reference_objective")
            except (OSError, json.JSONDecodeError):
                ref_objective = None
        if entry:
            times = ", ".join(f"{s} {entry[s]}" for s in
                              [c for c in entry if c not in
                               ("instance", "url", "accessed")])
            entry_text = f"p_{name} (milp_12threads.csv)"
        else:
            times = "-"
            entry_text = "no exact name match"
        if ref_objective is not None:
            obj_text = repr(ref_objective)
            verified = "yes (provenance reference_objective)"
        else:
            obj_text = "-"
            verified = "unverified (no published/provenance objective)"
        lines.append(f"| {name} | {entry_text} | {times} | {obj_text} | {verified} |")
    return "\n".join(lines) + "\n"


def parse_mittelmann_reference(path: str) -> dict:
    entries = {}
    if not os.path.isfile(path):
        return entries
    with open(path) as f:
        lines = f.read().splitlines()
    header = None
    for idx, line in enumerate(lines):
        if line.startswith("| instance | published entry |"):
            header = [c.strip() for c in line.strip("|").split("|")]
            for row in lines[idx + 2:]:
                if not row.startswith("|"):
                    break
                cells = [c.strip() for c in row.strip("|").split("|")]
                if len(cells) >= len(header):
                    entries[cells[0]] = dict(zip(header, cells))
            break
    return entries


def fmt(value) -> str:
    if value in (None, ""):
        return "-"
    try:
        return f"{float(value):.6g}"
    except (TypeError, ValueError):
        return str(value)


def build_report(args, probes, rows, instances, refs, markov_optimal, disagreements,
                  aggregates_rows, reference_entries, wall_seconds, svg_path,
                  csv_path, profile_data_path, versions, qp_diagnostics) -> str:
    total = len(rows)
    optimal_total = sum(1 for r in rows if r["status"] == "Optimal")
    verified_total = sum(1 for r in rows if r.get("verified"))
    available = [s for s in SOLVER_ORDER if probes.get(s, {}).get("available")]
    geomeans = {}
    for solver in available:
        if solver == "markov-cero":
            continue
        ratios = []
        for inst in [n for _, _, n in instances]:
            mc = next((r for r in rows if r["instance"] == inst
                       and r["solver"] == "markov-cero"
                       and r["status"] == "Optimal"), None)
            other = next((r for r in rows if r["instance"] == inst
                          and r["solver"] == solver
                          and r["status"] == "Optimal"), None)
            if mc and other:
                t_mc = float(mc["runtime_ms"])
                t_other = float(other["runtime_ms"])
                if t_mc > 0 and t_other > 0:
                    ratios.append(t_other / t_mc)
        geomeans[solver] = geomean(ratios)

    lines = []
    lines.append("# Comprehensive solver comparison (W9 / M6-37 multi-solver + Dolan-More)")
    lines.append("")
    lines.append(f"- Generated: {datetime.datetime.now().isoformat(timespec='seconds')} "
                 f"by `scripts/run_full_compare.py` (milestone-6 feature 37, Gap 6)")
    lines.append(f"- Host cores: {os.cpu_count()}; harness wall clock: "
                 f"{wall_seconds / 60.0:.1f} min")
    lines.append(f"- markov-cero binary: `{os.path.relpath(args.solver, REPO)}` "
                 f"(version {versions.get('markov-cero', 'unknown')}, engine auto)")
    lines.append(f"- markov-cero SHA-256: `{args.solver_sha256}`")
    lines.append(f"- Per-instance per-solver time cap: {args.timeout:g} s")
    lines.append(f"- Solver threads per run: {args.threads}")
    suite_counts = {suite: sum(1 for row_suite, _, _ in instances
                               if row_suite == suite)
                    for suite in {row_suite for row_suite, _, _ in instances}}
    class_summary = ", ".join(
        f"{suite} {count}" for suite, count in sorted(suite_counts.items()))
    lines.append(f"- Curated instances: {len(instances)} ({class_summary})")
    artifact_dir = os.path.relpath(os.path.dirname(csv_path), REPO)
    lines.append(f"- Results CSV: `{os.path.relpath(csv_path, REPO)}` "
                 f"({total} rows)")
    lines.append(f"- Dolan-More SVG: `{os.path.relpath(svg_path, REPO)}`")
    lines.append(f"- Dolan-More data: `{os.path.relpath(profile_data_path, REPO)}`")
    lines.append("- Suite exports: " + ", ".join(
        f"`{artifact_dir}/{name}`" for name in (
            "netlib_lp_comparison.csv", "miplib_comparison.csv",
            "mittelmann_lp_comparison.csv", "mittelmann_milp_comparison.csv",
            "qplib_comparison.csv", "dolan_more_lp.svg", "dolan_more_milp.svg")) + ".")
    lines.append("")

    lines.append("## 1. Solver availability")
    lines.append("")
    lines.append("| solver | available | version | probe | notes |")
    lines.append("|---|---|---|---|---|")
    for name in SOLVER_ORDER:
        info = probes.get(name, {"available": False, "version": "unknown",
                                 "how": "-", "notes": GLPK_NOTE})
        flag = "yes" if info.get("available") else "**no**"
        lines.append(f"| {name} | {flag} | {info.get('version', 'unknown')} | "
                     f"{info.get('how', '-')} | {info.get('notes') or '-'} |")
    lines.append("")
    lines.append("Every solver is optional: a solver that cannot run on this host is "
                 "reported here as unavailable and produces no result rows. GLPK "
                 "runs as an external, serial `glpsol` process when available.")
    lines.append("")

    lines.append("## 2. Aggregate results")
    lines.append("")
    lines.append("| solver | rows | optimal | feasible | timeout/error | unsupported | "
                 "verified | disagreements | geomean(t / t_markov) |")
    lines.append("|---|---|---|---|---|---|---|---|---|")
    for entry in aggregates_rows:
        solver = entry["solver"]
        gm = geomeans.get(solver, float("nan"))
        gm_text = "1.000 (self)" if solver == "markov-cero" else (
            f"{gm:.3f}" if math.isfinite(gm) else "n/a")
        dis_count = sum(1 for d in disagreements
                        if solver in str(d.get("solver", "")))
        lines.append(f"| {solver} | {entry['rows']} | {entry['optimal']} | "
                     f"{entry['feasible']} | {entry['other']} | "
                     f"{entry['unsupported']} | {entry['verified']} | "
                     f"{dis_count} | "
                     f"{gm_text} |")
    lines.append("")
    lines.append(f"- Total rows: {total}; optimal runs: {optimal_total}/{total}; "
                 f"verified objectives: {verified_total}/{total}")
    lines.append(f"- **Disagreements found: {len(disagreements)}** (pairs of optimal "
                 f"objectives whose relative difference exceeds {AGREE_TOL:g})")
    if disagreements:
        lines.append("")
        lines.append("| instance | solver | compared against | solver objective | "
                     "reference objective | rel diff |")
        lines.append("|---|---|---|---|---|---|")
        for d in disagreements:
            lines.append(f"| {d['instance']} | {d['solver']} | {d['against']} | "
                         f"{fmt(d['solver_objective'])} | "
                         f"{fmt(d['reference_objective'])} | "
                         f"{d['rel_diff']:.3e} |")
    else:
        lines.append("  (no pair of optimal objectives disagreed beyond the "
                     f"{AGREE_TOL:g} relative tolerance)")
    if qp_diagnostics:
        lines.append("")
        lines.append("### Disagreement diagnostics: QUADOBJ off-diagonal sweep")
        lines.append("")
        lines.append("`scripts/check_qp_convention.py` rewrites the QUADOBJ "
                     "off-diagonal entries with an explicit multiplier and re-solves "
                     "each variant, so the semantics each solver applies can be "
                     "identified rather than guessed:")
        lines.append("")
        for inst, text in sorted(qp_diagnostics.items()):
            lines.append(f"```text")
            lines.append(text)
            lines.append("```")
    lines.append("")
    lines.append("`geomean(t / t_markov)` is the geometric mean of "
                 "`runtime(solver) / runtime(markov-cero)` over the instances where "
                 "both solvers reached `Optimal`; values above 1 mean the solver is "
                 "slower than markov-cero. The inverse direction "
                 "(`markov-cero / solver`, the convention used by the legacy W9 "
                 "report) is simply `1 / value`.")
    lines.append("")

    lines.append("## 3. Per-instance results")
    lines.append("")
    lines.append("Cell format: `status · objective · runtime · verified` "
                 "(`-` = no certified objective).")
    lines.append("")
    header_cells = ["instance", "class"] + [s for s in SOLVER_ORDER
                                            if probes.get(s, {}).get("available")]
    lines.append("| " + " | ".join(header_cells) + " |")
    lines.append("|" + "---|" * len(header_cells))
    for suite, cls, name in instances:
        cells = [name, cls]
        for solver in header_cells[2:]:
            row = next((r for r in rows if r["instance"] == name
                        and r["solver"] == solver), None)
            if row is None:
                cells.append("-")
                continue
            mark = "yes" if row.get("verified") else "no"
            cells.append(f"{row['status']} · {fmt(row['objective'])} · "
                         f"{row['runtime_ms']:g} ms · {mark}")
        lines.append("| " + " | ".join(cells) + " |")
    lines.append("")

    lines.append("## 4. Dolan-More performance profile")
    lines.append("")
    lines.append("Generated by `scripts/dolan_more_profile.py` "
                 "(hand-rolled SVG, pure Python, no matplotlib):")
    lines.append("")
    lines.append(f"- `{os.path.relpath(svg_path, REPO)}`")
    lines.append(f"- `{os.path.relpath(profile_data_path, REPO)}` "
                 "(sampled `tau, solver, rho` points)")
    lines.append("")
    lines.append("Exact formulation used (no approximation, no smoothing):")
    lines.append("")
    lines.append("    r(p,s) = t(p,s) / min over s' of t(p,s'),   min over solvers "
                 "s' that solved p")
    lines.append("    r(p,s) = +inf when solver s failed or timed out on p")
    lines.append("    if s solved p and no other solver did, min = t(p,s) so "
                 "r(p,s) = 1 exactly")
    lines.append("    rho_s(tau) = (1 / |P|) * |{ p in P : r(p,s) <= tau }|")
    lines.append("")
    lines.append(f"`|P| = {len(instances)}` curated problems (problems nobody solved "
                 "stay in the denominator and contribute 0 to every curve). A solver "
                 "is a \"success\" on p exactly when its status in "
                 "`full_compare_results.csv` is `Optimal`; timeouts, feasible-but-"
                 "unproven incumbents, errors and unsupported models are failures "
                 "with `r = +inf`. The x axis is log-scaled "
                 "(`tau` from 1 to 100, decades), the y axis is `rho_s(tau)` from "
                 "0 to 1, one polyline per solver with a legend.")
    lines.append("")

    lines.append("## 5. Mittelmann published-reference cross-check")
    lines.append("")
    lines.append(f"Reference document: `{artifact_dir}/mittelmann_reference.md` "
                 "(published Mittelmann tables, regenerated by this script and "
                 "extended with per-instance entries). The table below compares the "
                 "markov-cero objectives on the `data/mittelmann` instances against "
                 "the reference entries **where instance names match**.")
    lines.append("")
    lines.append("| instance | markov-cero status | markov-cero objective | "
                 "reference entry (name match) | reference objective | "
                 "objective rel-diff | verification |")
    lines.append("|---|---|---|---|---|---|---|")
    for suite, _, name in instances:
        if suite != "mittelmann":
            continue
        mc_row = next((r for r in rows if r["instance"] == name
                       and r["solver"] == "markov-cero"), None)
        entry = reference_entries.get(name, {})
        entry_text = entry.get("published entry", "-")
        ref_objective_text = entry.get("reference objective", "-")
        mc_status = mc_row["status"] if mc_row else "not run"
        mc_objective = fmt(mc_row.get("objective")) if mc_row else "-"
        if ref_objective_text not in ("-", "", None):
            try:
                rel = rel_diff(float(mc_objective), float(ref_objective_text))
                rel_text = f"{rel:.3e}"
                verdict = ("verified" if rel <= AGREE_TOL
                           else f"DISAGREEMENT (>{AGREE_TOL:g})")
            except (TypeError, ValueError):
                rel_text = "n/a"
                verdict = "unverified (reference objective not numeric)"
        else:
            rel_text = "n/a"
            verdict = ("unverified: published tables record wall-clock times only; "
                       "no published objective for this instance")
        lines.append(f"| {name} | {mc_status} | {mc_objective} | {entry_text} | "
                     f"{ref_objective_text} | {rel_text} | {verdict} |")
    lines.append("")
    measured = []
    for suite, _, name in instances:
        if suite != "mittelmann":
            continue
        opts = [r for r in rows if r["instance"] == name and r["status"] == "Optimal"]
        if opts:
            measured.append(f"- **{name}**: " + ", ".join(
                f"{r['solver']} {fmt(r['objective'])}" for r in opts)
                + ("" if len(opts) > 1 else " (single solver reached optimality)"))
        else:
            measured.append(f"- **{name}**: no solver proved optimality within "
                            f"{args.timeout:g} s (all rows are timeouts/errors)")
    lines.append("Measured cross-check from this run (not a published reference):")
    lines.append("")
    lines += measured
    lines.append("")
    lines.append("Reference objectives for the `data/mittelmann` set are **absent** "
                 "in every source in this repository (provenance files carry no "
                 "`reference_objective`, and the published Mittelmann tables only "
                 "publish wall-clock times), so every row above is honestly marked "
                 "unverified. Where the published table does contain a name-matched "
                 "entry (`p_neos5`), its times are cited verbatim in "
                 "`mittelmann_reference.md`.")
    lines.append("")

    lines.append("## 6. Methodology, gaps and preserved artifacts")
    lines.append("")
    lines.append("- Measured runs share the curated instance set, the time cap "
                 f"({args.timeout:g} s), and the host. Published Mittelmann numbers "
                 "are cited, never mixed into the measured geomeans. Failures are "
                 "reported, never dropped.")
    lines.append(f"- The solvers of one instance run concurrently "
                 f"(pool cap {args.workers} workers for {len(available)} available "
                 f"solvers); each configurable solver uses {args.threads} thread(s), "
                 "while GLPK is serial. "
                 f"Wall clock of the "
                 f"whole matrix: {wall_seconds / 60.0:.1f} min.")
    lines.append("- Timing asymmetry (inherited from the W9 harness): markov-cero is "
                 "timed as a full subprocess (spawn + model read + solve), the Python "
                 "API solvers are timed over model load + solve.")
    lines.append("- QP inputs: markov-cero and HiGHS read `QUADOBJ` MPS directly; CBC "
                 "and GLPK cannot handle quadratic objectives (rows are `Unsupported`); SCIP's "
                 "MPS reader requires `QUADOBJ` after `BOUNDS` and an `RHS` section, "
                 "so for SCIP only the file is reformatted into a temporary copy "
                 "(sections reordered, empty `RHS` injected when missing) - the "
                 "mathematics of the file is unchanged.")
    lines.append("- `verified` = status `Optimal` AND relative agreement "
                 f"({AGREE_TOL:g}) with an independent objective: the provenance "
                 "`reference_objective` when the instance has one, otherwise markov-"
                 "cero's certificate-backed objective. `verified=no` therefore means "
                 "\"not independently checked\", not \"wrong\".")
    lines.append("- Preserved untouched: `evidence/comparison/netlib_comparison.csv` "
                 "and `evidence/comparison/miplib_comparison.csv` (W9 legacy schema, "
                 "add-don't-break); "
                 f"`{os.path.relpath(csv_path, REPO)}` supersedes them for the "
                 "curated multi-solver set.")
    lines.append("")
    legacy = os.path.join(OUT, "full_comparison_report.md")
    if not args.drop_legacy_report and os.path.isfile(legacy):
        lines.append("### Appendix: previous W9 report (kept for continuity)")
        lines.append("")
        with open(legacy) as f:
            legacy_text = f.read().strip()
        if legacy_text.startswith("# Comprehensive solver comparison"):
            legacy_lines = legacy_text.splitlines()
            lines.append("> " + "\n> ".join(legacy_lines[:1]))
            lines.append(">")
            for line in legacy_lines[1:]:
                lines.append("> " + line)
    lines.append("")
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--solver",
                    default=os.path.join(REPO, "build_gap", "markov-cero-solve"))
    ap.add_argument("--timeout", type=float, default=60.0,
                    help="per-instance per-solver time cap in seconds")
    ap.add_argument("--workers", type=int, default=4,
                    help="solvers of one instance run concurrently")
    ap.add_argument("--threads", type=int, default=1,
                    help="matched solver threads per run (default: 1)")
    ap.add_argument("--instances", nargs="*", default=None,
                    help="optional instance names to run (default: curated set)")
    ap.add_argument("--suites", nargs="*", default=None,
                    choices=sorted({suite for suite, _, _ in CURATED}),
                    help="optional suites to include (default: all curated suites)")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--skip-profile", action="store_true")
    ap.add_argument("--drop-legacy-report", action="store_true",
                    help="do not quote the previous report as an appendix")
    args = ap.parse_args()

    ensure_runtime_env()
    os.makedirs(args.out, exist_ok=True)

    instances = []
    for suite, cls, name in CURATED:
        if args.instances is not None and name not in args.instances:
            continue
        if args.suites is not None and suite not in args.suites:
            continue
        path = mps_path(suite, name)
        if os.path.isfile(path):
            instances.append((suite, cls, name))
        else:
            print(f"[!] missing instance {path}, skipped", file=sys.stderr)
    if not instances:
        print("[-] no curated instances found", file=sys.stderr)
        return 2

    probes = probe_availability(args.solver)
    args.solver_sha256 = ""
    if os.path.isfile(args.solver):
        with open(args.solver, "rb") as binary_file:
            args.solver_sha256 = hashlib.file_digest(binary_file, "sha256").hexdigest()
    print("[+] availability: " + ", ".join(
        f"{name}={'yes' if probes[name]['available'] else 'no'}"
        for name in SOLVER_ORDER))

    runnable = [s for s in SOLVER_ORDER if probes.get(s, {}).get("available")]
    if not runnable:
        print("[-] no solver available", file=sys.stderr)
        return 2

    rows = []
    started = time.perf_counter()
    # Each task spends its time in a child solver process or a native solver
    # API that releases the GIL. Threads avoid multiprocessing's forkserver,
    # which is unavailable in restricted/containerized environments.
    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
        for suite, cls, name in instances:
            if args.solver_sha256:
                with open(args.solver, "rb") as binary_file:
                    current_sha256 = hashlib.file_digest(binary_file, "sha256").hexdigest()
                if current_sha256 != args.solver_sha256:
                    print("[-] markov-cero binary changed during comparison run",
                          file=sys.stderr)
                    return 2
            path = mps_path(suite, name)
            futures = [pool.submit(solve_task, {
                "solver": solver, "instance": name, "mps": path,
                "timeout": args.timeout, "threads": args.threads,
                "binary": args.solver})
                for solver in runnable]
            for future in futures:
                try:
                    rows.append(future.result(timeout=args.timeout + 45))
                except Exception as exc:
                    rows.append({"instance": name, "solver": "unknown",
                                 "status": f"Error({type(exc).__name__})",
                                 "objective": "", "runtime_ms": 0.0,
                                 "certified": False, "detail": str(exc)})
            done = len([r for r in rows if r["instance"] == name])
            print(f"  [{done:3d}/{len(instances) * len(runnable)}] {suite}/{name}")
    for row in rows:
        row["markov_cero_sha256"] = args.solver_sha256
    wall_seconds = time.perf_counter() - started

    order = {name: i for i, (_, _, name) in enumerate(instances)}
    solver_order = {s: i for i, s in enumerate(SOLVER_ORDER)}
    rows.sort(key=lambda r: (order.get(r["instance"], 999),
                             solver_order.get(r["solver"], 999)))

    versions = {r["solver"]: r.get("version") or probes.get(
        r["solver"], {}).get("version", "unknown")
        for r in rows if r.get("version")}
    if "markov-cero" in versions:
        probes["markov-cero"]["version"] = versions["markov-cero"]

    refs = load_reference_objectives(instances)
    _, disagreements = annotate(rows, refs, instances)
    aggregates_rows = aggregates(rows, probes)

    csv_path = os.path.join(args.out, "full_compare_results.csv")
    write_results_csv(csv_path, rows)
    print(f"[+] {len(rows)} rows -> {csv_path}")
    plan_exports = write_plan_comparison_exports(args.out, rows, instances, refs)
    if os.path.abspath(args.out) == os.path.abspath(OUT):
        # Preserve the pre-existing W9 tables before updating their filenames
        # to the plan's all-solver schema.
        for filename in ("netlib_comparison.csv", "miplib_comparison.csv"):
            path = os.path.join(args.out, filename)
            legacy = os.path.join(args.out, filename[:-4] + "_legacy.csv")
            if os.path.isfile(path) and not os.path.exists(legacy):
                shutil.copy2(path, legacy)
    print("[+] plan-facing suite tables -> " + ", ".join(
        os.path.join(args.out, name) for name in plan_exports))

    reference_path = os.path.join(args.out, "mittelmann_reference.md")
    with open(reference_path, "w") as f:
        f.write(write_mittelmann_reference(instances))
    print(f"[+] reference -> {reference_path}")
    reference_entries = parse_mittelmann_reference(reference_path)

    svg_path = os.path.join(args.out, "dolan_more_runtime_profile.svg")
    profile_data_path = os.path.join(args.out, "dolan_more_profile_data.csv")
    profile_ok = False
    if not args.skip_profile:
        profile = subprocess.run(
            [sys.executable, os.path.join(SCRIPTS, "dolan_more_profile.py"),
             "--input", csv_path, "--svg", svg_path, "--csv", profile_data_path],
            capture_output=True, text=True, cwd=REPO)
        sys.stdout.write(profile.stdout)
        if profile.returncode != 0:
            sys.stderr.write(profile.stderr)
            print(f"[-] profile generation failed (rc={profile.returncode})",
                  file=sys.stderr)
        else:
            profile_ok = True

    markov_optimal = {r["instance"]: r["objective"] for r in rows
                      if r["solver"] == "markov-cero" and r["status"] == "Optimal"}
    report = build_report(args, probes, rows, instances, refs, markov_optimal,
                          disagreements, aggregates_rows, reference_entries,
                          wall_seconds,
                          svg_path if profile_ok else "(not generated)",
                          csv_path, profile_data_path, versions, {})
    report_path = os.path.join(args.out, "full_comparison_report.md")
    with open(report_path, "w") as f:
        f.write(report)
    print(f"[+] report -> {report_path}")

    print(f"[+] wall clock {wall_seconds / 60.0:.1f} min; "
          f"optimal {sum(1 for r in rows if r['status'] == 'Optimal')}/{len(rows)}; "
          f"verified {sum(1 for r in rows if r.get('verified'))}/{len(rows)}; "
          f"disagreements {len(disagreements)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
