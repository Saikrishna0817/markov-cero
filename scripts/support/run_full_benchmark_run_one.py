from .run_full_benchmark_config import (
    Path, REPO, hashlib, json, subprocess, time
)

def qplib_convexity_gate(mps: Path) -> tuple[bool, str]:
    """Admit convex continuous QPs with linear or box constraints only.

    QPLIB's first class letter C/D guarantees a convex quadratic objective.
    Q-coded objectives are only admitted after an explicit dense PSD check;
    this is limited to small matrices so an unverified large model is never
    silently called convex. The third class letter must describe linear or
    bound-only constraints, which is the subset this QP engine supports.
    """
    provenance_path = mps.with_suffix(".provenance.json")
    try:
        provenance = json.loads(provenance_path.read_text())
        code = str(provenance.get("problem_class", ""))
        n = int(provenance.get("columns", 0))
        direction = str(provenance.get("direction", "minimize")).lower()
    except (OSError, ValueError, json.JSONDecodeError):
        return False, "missing or invalid QPLIB provenance/class code"
    if len(code) != 3 or code[1] != "C":
        return False, f"variable class {code[1:2]!r} is not continuous"
    if code[2] not in {"L", "B"}:
        return False, f"constraint class {code!r} is outside linear/bounds-only QP scope"
    if code[0] in {"C", "D"}:
        return True, f"QPLIB {code} convex-objective class with supported constraints"
    if code[0] != "Q":
        return False, f"unsupported objective class {code!r}"
    if n <= 0 or n > 1024:
        return False, f"Q-coded objective dimension {n} exceeds explicit PSD-check limit"

    import numpy as np
    from scripts.check_qp_convention import read_quadratics

    _, entries = read_quadratics(str(mps))
    hessian = np.zeros((n, n), dtype=np.float64)
    for _, col_i, col_j, value in entries:
        try:
            i, j = int(col_i[1:]) - 1, int(col_j[1:]) - 1
        except (ValueError, IndexError):
            return False, "cannot map QPLIB quadratic variable names for PSD check"
        if not (0 <= i < n and 0 <= j < n):
            return False, "quadratic variable index is outside provenance dimensions"
        hessian[i, j] += value
        if i != j:
            hessian[j, i] += value
    if direction == "maximize":
        hessian *= -1.0
    eigenvalues = np.linalg.eigvalsh(hessian)
    scale = max(1.0, float(np.max(np.abs(eigenvalues))))
    if float(eigenvalues[0]) < -1e-10 * scale:
        return False, f"objective Hessian is not PSD (minimum eigenvalue {eigenvalues[0]:.6g})"
    return True, f"Q-coded objective passed dense PSD check (min eigenvalue {eigenvalues[0]:.6g})"

def run_one(solver: str, mps: Path, suite: str, engine: str,
            timeout: float, threads: int, expected_sha256: str = "") -> dict:
    provenance_path = REPO / "data" / suite / f"{mps.stem}.provenance.json"
    best_known = ""
    if provenance_path.is_file():
        try:
            reference = json.loads(provenance_path.read_text()).get("reference_objective")
            if isinstance(reference, (int, float)):
                best_known = float(reference)
        except (OSError, json.JSONDecodeError):
            pass
    cmd = [solver, str(mps)]
    if engine != "auto":
        cmd += ["--engine", engine]
    cmd += ["--time-limit", str(timeout), "--threads", str(threads)]
    started = time.time()
    row = {
        "solver": "markov-cero",
        "solver_sha256": hashlib.sha256(Path(solver).read_bytes()).hexdigest(),
        "suite": suite,
        "instance": mps.stem,
        "problem_class": "",
        "n_vars": "",
        "n_constraints": "",
        "n_nonzeros": "",
        "status": "Error",
        "verified": False,
        "objective": "",
        "best_known_obj": best_known,
        "obj_error": "",
        "time_ms": 0.0,
        "nodes": "",
        "gap": "",
        "kkt_primal": "",
        "kkt_dual": "",
    }
    if not mps.is_file():
        row["status"] = "DatasetUnavailable"
        return row
    if expected_sha256 and row["solver_sha256"].lower() != expected_sha256.lower():
        row["status"] = "BinaryChanged"
        return row
    try:
        # The CLI/API deadline is the solve cap. Keep five seconds for the
        # process to unwind and serialize its ResourceLimit result; this parent
        # watchdog only catches non-cooperative or stuck code paths.
        proc = subprocess.run(cmd, capture_output=True, timeout=timeout + 5.0)
        row["time_ms"] = round((time.time() - started) * 1000.0, 1)
        payload = None
        for line in reversed(proc.stdout.decode("utf-8", "replace").splitlines()):
            try:
                candidate = json.loads(line)
                if isinstance(candidate, dict) and "status" in candidate:
                    payload = candidate
                    break
            except json.JSONDecodeError:
                continue
        if payload is not None:
            row["status"] = payload.get("status", "?")
            row["verified"] = bool(payload.get("verified", False))
            primal = payload.get("primal")
            objective = payload.get("objective", "")
            has_primal = isinstance(primal, list) and len(primal) > 0
            row["objective"] = (objective if row["status"] == "Optimal" or has_primal
                                else "")
            row["problem_class"] = payload.get("problem_class", "")
            row["n_vars"] = payload.get("cols", "")
            row["n_constraints"] = payload.get("rows", "")
            row["n_nonzeros"] = payload.get("nonzeros", "")
            row["nodes"] = payload.get("nodes_explored", "")
            if row["status"] == "Optimal" and row["verified"]:
                row["gap"] = payload.get("relative_gap", "")
                row["kkt_primal"] = payload.get("maximum_canonical_primal_violation", "")
                row["kkt_dual"] = payload.get("maximum_canonical_dual_violation", "")
            if (row["status"] == "Optimal" and row["verified"]
                    and best_known != "" and isinstance(row["objective"], (int, float))):
                row["obj_error"] = abs(float(row["objective"]) - best_known) / max(1.0, abs(best_known))
        else:
            row["status"] = f"Exit{proc.returncode}"
    except subprocess.TimeoutExpired:
        row["time_ms"] = round((time.time() - started) * 1000.0, 1)
        row["status"] = "Timeout"
    except json.JSONDecodeError:
        row["status"] = "BadJSON"
    return row
