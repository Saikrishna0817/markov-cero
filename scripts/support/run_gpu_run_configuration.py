from .run_gpu_config import (
    Any, Dict, List, Optional, json, math, subprocess, time
)

def run_configuration(
    solver: str,
    mps_path: str,
    engine: str,
    backend: Optional[str] = None,
    tolerance: float = 1e-4,
    timeout: int = 120,
) -> Dict[str, Any]:
    cmd = [solver, mps_path, "--engine", engine]
    if backend:
        cmd.extend(["--backend", backend])
    if engine == "pdlp":
        cmd.extend(["--tolerance", str(tolerance)])

    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        wall_ms = (time.perf_counter() - t0) * 1000.0
    except subprocess.TimeoutExpired:
        return {
            "exit_code": -1,
            "status": "Timeout",
            "verified": False,
            "objective": float("nan"),
            "iterations": 0,
            "time_ms": timeout * 1000.0,
            "h2d_ms": 0.0,
            "kernel_ms": 0.0,
            "d2h_ms": 0.0,
            "total_ms": timeout * 1000.0,
            "error": f"Timed out after {timeout}s",
        }

    parsed = None
    for line in proc.stdout.splitlines():
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            try:
                parsed = json.loads(line)
                break
            except Exception:
                pass

    if parsed is None:
        return {
            "exit_code": proc.returncode,
            "status": "ParseError",
            "rows": 0,
            "cols": 0,
            "nonzeros": 0,
            "verified": False,
            "objective": float("nan"),
            "iterations": 0,
            "time_ms": wall_ms,
            "h2d_ms": 0.0,
            "kernel_ms": 0.0,
            "d2h_ms": 0.0,
            "total_ms": wall_ms,
            "error": proc.stderr[:200],
        }

    iters = parsed.get("lp_iterations", parsed.get("total_iterations", 0))
    time_ms = parsed.get("total_ms", parsed.get("runtime_ms", wall_ms))
    return {
        "exit_code": proc.returncode,
        "status": parsed.get("status", "Unknown"),
        "rows": parsed.get("rows", 0),
        "cols": parsed.get("cols", 0),
        "nonzeros": parsed.get("nonzeros", 0),
        "verified": parsed.get("verified", False),
        "objective": parsed.get("objective", float("nan")),
        "iterations": iters,
        "time_ms": time_ms,
        "h2d_ms": parsed.get("h2d_ms", 0.0),
        "kernel_ms": parsed.get("kernel_ms", 0.0),
        "d2h_ms": parsed.get("d2h_ms", 0.0),
        "total_ms": parsed.get("total_ms", time_ms),
        "error": parsed.get("error", ""),
    }

def configuration_failures(
    label: str, res: Dict[str, Any], allow_skipped: bool = False
) -> List[str]:
    """A configuration passes only when it terminates Optimal and verifies."""
    if allow_skipped and res["status"] == "Skipped":
        return []
    if res["status"] != "Optimal":
        return [f"{label}={res['status']}"]
    if not res["verified"]:
        return [f"{label}=Optimal(unverified)"]
    return []

def failed_record(
    instance: str,
    rows: int,
    cols: int,
    reference: float,
    failures: str,
    hardware_id: str,
) -> Dict[str, Any]:
    nan = float("nan")
    return {
        "instance": instance.upper(),
        "rows": rows,
        "cols": cols,
        "nonzeros": 0,
        "reference_objective": reference,
        "simplex_status": "NotRun",
        "simplex_verified": False,
        "simplex_objective": nan,
        "simplex_iterations": 0,
        "simplex_time_ms": nan,
        "cpu_pdlp_status": "NotRun",
        "cpu_pdlp_verified": False,
        "cpu_pdlp_objective": nan,
        "cpu_pdlp_iterations": 0,
        "cpu_pdlp_time_ms": nan,
        "gpu_pdlp_status": "NotRun",
        "gpu_pdlp_objective": nan,
        "gpu_pdlp_iterations": 0,
        "gpu_h2d_ms": 0.0,
        "gpu_kernel_ms": 0.0,
        "gpu_d2h_ms": 0.0,
        "gpu_total_ms": 0.0,
        "speedup_kernel": nan,
        "speedup_end_to_end": nan,
        "speedup_vs_simplex": nan,
        "gpu_verified": False,
        "failures": failures,
        "hardware_id": hardware_id,
        "pass": False,
    }

def fmt_speedup(value: float) -> str:
    if value is None or (isinstance(value, float) and math.isnan(value)) or value <= 0:
        return "N/A"
    return f"{value:.2f}x"
