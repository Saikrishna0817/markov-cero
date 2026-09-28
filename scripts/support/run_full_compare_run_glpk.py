from __future__ import annotations
from .run_full_compare_config import (
    REPO, math, os, re, subprocess, tempfile, time
)
from .run_full_compare_run_highs import find_glpsol
from .run_full_compare_run_highs import has_quadratic_objective
from .run_full_compare_run_highs import parse_cbc_output
from .run_full_compare_run_highs import prepare_mps_for_scip
from .run_full_compare_run_highs import run_highs
from .run_full_compare_run_highs import run_markov

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
        elif solver in {"GLPK", "CBC", "SCIP"}:
            from .oracle_process import run_solver_process
            result = run_solver_process(solver, payload["mps"], payload["timeout"], payload["threads"])
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

def pulp_version() -> str:
    try:
        import pulp
        return getattr(pulp, "__version__", "unknown")
    except Exception:
        return "unavailable"
