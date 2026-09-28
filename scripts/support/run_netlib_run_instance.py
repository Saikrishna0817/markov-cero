from .run_netlib_config import (
    Any, BASE_URL, Dict, Optional, gzip, json, os, subprocess, time, urllib
)

def find_solver_binary() -> Optional[str]:
    candidates = [
        "_deployment-phase2-build/markov-cero-solve",
        "build/markov-cero-solve",
        "_build/markov-cero-solve",
        "markov-cero-solve",
    ]
    for c in candidates:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    return None

def ensure_instance(name: str, data_dir: str) -> str:
    target = os.path.join(data_dir, f"{name}.mps")
    if not os.path.isfile(target) or os.path.getsize(target) == 0:
        raise FileNotFoundError(
            f"DatasetUnavailable: {target}; explicitly run scripts/datasets.py --name {name}")
    return target


def run_instance(
    solver_bin: str,
    mps_path: str,
    engine: str = "primal",
    timeout_sec: int = 120,
) -> Dict[str, Any]:
    cmd = [solver_bin, mps_path, "--engine", engine]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_sec)
        wall_ms = (time.perf_counter() - t0) * 1000.0
    except subprocess.TimeoutExpired:
        return {
            "exit_code": -1,
            "status": "Timeout",
            "verified": False,
            "error": f"Execution timed out after {timeout_sec}s",
            "wall_ms": timeout_sec * 1000.0,
        }

    parsed_json = None
    for line in proc.stdout.splitlines():
        line_str = line.strip()
        if line_str.startswith("{") and line_str.endswith("}"):
            try:
                parsed_json = json.loads(line_str)
                break
            except Exception:
                pass

    if parsed_json is None:
        return {
            "exit_code": proc.returncode,
            "status": "ParseError",
            "verified": False,
            "error": f"No valid JSON output. Stderr: {proc.stderr[:200]}",
            "stdout": proc.stdout[:200],
            "wall_ms": wall_ms,
        }

    parsed_json["exit_code"] = proc.returncode
    parsed_json["wall_ms"] = wall_ms
    return parsed_json
