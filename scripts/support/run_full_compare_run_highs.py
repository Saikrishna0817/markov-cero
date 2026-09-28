from __future__ import annotations
from .run_full_compare_config import (
    MPS_SECTIONS, REPO, _ENTRY_POINT, importlib, json, os, re, shutil, subprocess, sys, tempfile, time
)

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
            os.execv(venv_py, [venv_py, os.path.abspath(_ENTRY_POINT)] + sys.argv[1:])
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
            [binary, mps, "--time-limit", str(timeout),
             "--threads", str(int(threads))],
            capture_output=True, text=True, timeout=timeout + 10)
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
    from .oracle_process import run_highs_process
    return run_highs_process(mps, timeout, threads)

def _run_highs_in_process(mps: str, timeout: float, threads: int = 1) -> dict:
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
