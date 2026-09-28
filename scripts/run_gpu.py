#!/usr/bin/env python3
"""
markov-cero GPU Benchmark Runner
Three-way algorithmic evaluation: CPU Simplex vs CPU PDLP vs GPU PDLP.
Generates a consolidated, reproducible CSV reporting four-part GPU timing (D-GPU-08).
Records machine hardware (RW-9) in the report header and a JSON sidecar, and fails
non-zero whenever any selected instance fails any of its three configurations.
"""

import argparse
import csv
import datetime
import hashlib
import json
import math
import os
import platform
import shutil
import subprocess
import sys
import textwrap
import time
from typing import Dict, Any, List, Optional

# Canonical reference benchmarks with optimal objective values
BENCHMARKS = {
    "afiro": {"rows": 28, "cols": 32, "optimal": -464.753142857143},
    "blend": {"rows": 75, "cols": 84, "optimal": -30.8121498457},
    "adlittle": {"rows": 57, "cols": 97, "optimal": 225494.96316238},
    "sc50a": {"rows": 51, "cols": 48, "optimal": -64.575077058564},
    "sc50b": {"rows": 51, "cols": 48, "optimal": -70.000000000000},
    "sc105": {"rows": 106, "cols": 103, "optimal": -52.202061211707},
    "sc205": {"rows": 206, "cols": 203, "optimal": -52.202061211707},
    "share2b": {"rows": 97, "cols": 79, "optimal": -415.732240741418},
    "share1b": {"rows": 118, "cols": 225, "optimal": -76589.3185791855},
    "recipe": {"rows": 92, "cols": 180, "optimal": -266.616000000000},
    "scsd1": {"rows": 78, "cols": 760, "optimal": 8.66666667},
    "scsd6": {"rows": 148, "cols": 1350, "optimal": 50.5},
    "beaconfd": {"rows": 174, "cols": 262, "optimal": 33592.4858072000},
    "scorpion": {"rows": 389, "cols": 358, "optimal": 1878.1248227381},
    # Scale-study instances (data/scale_study, objectives per
    # evidence/benchmarks/crossover_study.csv reference_objective column)
    "scale_5": {"rows": 6, "cols": 8, "optimal": 9.999981268643653},
    "scale_10": {"rows": 9, "cols": 12, "optimal": 9.999981268643653},
    "scale_20": {"rows": 16, "cols": 24, "optimal": 14.999975797264936},
    "scale_35": {"rows": 25, "cols": 40, "optimal": 19.99997166131662},
    "scale_50": {"rows": 42, "cols": 70, "optimal": 24.999968666875546},
    "scale_75": {"rows": 56, "cols": 96, "optimal": 29.999966716397292},
    "scale_100": {"rows": 60, "cols": 100, "optimal": 24.999968666875546},
    "scale_200": {"rows": 126, "cols": 224, "optimal": 39.999965556864396},
    "scale_500": {"rows": 275, "cols": 500, "optimal": 49.99996766091429},
    "scale_1000": {"rows": 525, "cols": 980, "optimal": 69.99998065148145},
    "scale_2000": {"rows": 1050, "cols": 2000, "optimal": 100.0000494963771},
    "scale_5000": {"rows": 2550, "cols": 4950, "optimal": 165.00031099724058},
    "scale_10000": {"rows": 5100, "cols": 10000, "optimal": 250.00051979740016},
}

DEFAULT_INSTANCES = ["afiro", "blend", "sc50a", "sc50b", "adlittle", "scsd1"]

# Fixed CSV schema so a short-circuited (failed) record cannot desynchronise columns.
RECORD_FIELDS = [
    "instance", "rows", "cols", "nonzeros", "reference_objective",
    "simplex_status", "simplex_verified", "simplex_objective",
    "simplex_iterations", "simplex_time_ms",
    "cpu_pdlp_status", "cpu_pdlp_verified", "cpu_pdlp_objective",
    "cpu_pdlp_iterations", "cpu_pdlp_time_ms",
    "gpu_pdlp_status", "gpu_pdlp_objective", "gpu_pdlp_iterations",
    "gpu_h2d_ms", "gpu_kernel_ms", "gpu_d2h_ms", "gpu_total_ms",
    "speedup_kernel", "speedup_end_to_end", "speedup_vs_simplex",
    "gpu_verified", "failures", "hardware_id", "pass",
]

TABLE_WIDTH = 152


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


def _sh(cmd: List[str], timeout: int = 15):
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return proc.returncode, proc.stdout, proc.stderr
    except (OSError, subprocess.TimeoutExpired):
        return -1, "", ""


def detect_hardware(solver: str) -> Dict[str, Any]:
    """Collect machine hardware for the report header and the sidecar (RW-9)."""
    hw: Dict[str, Any] = {
        "timestamp_utc": datetime.datetime.now(datetime.timezone.utc)
        .isoformat(timespec="seconds"),
    }

    # GPU via nvidia-smi; absence is reported honestly, never fatal.
    gpus: List[Dict[str, str]] = []
    rc, out, _ = _sh([
        "nvidia-smi", "--query-gpu=name,memory.total,driver_version",
        "--format=csv,noheader",
    ])
    if rc == 0:
        for line in out.splitlines():
            parts = [p.strip() for p in line.split(",")]
            if not parts or not parts[0]:
                continue
            gpus.append({
                "name": parts[0],
                "memory_total": parts[1] if len(parts) > 1 else "unknown",
                "driver_version": parts[2] if len(parts) > 2 else "unknown",
            })
    hw["gpus"] = gpus
    hw["gpu_note"] = "" if gpus else (
        "no GPU detected (nvidia-smi unavailable or reported no devices)"
    )

    # CPU via /proc/cpuinfo, falling back to lscpu.
    cpu_model, cpu_threads = "", 0
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("processor"):
                    cpu_threads += 1
                elif not cpu_model and line.startswith("model name"):
                    cpu_model = line.split(":", 1)[1].strip()
    except OSError:
        pass
    if not cpu_model:
        rc, out, _ = _sh(["lscpu"])
        if rc == 0:
            for line in out.splitlines():
                if line.startswith("Model name:"):
                    cpu_model = line.split(":", 1)[1].strip()
                elif line.startswith("CPU(s):") and not cpu_threads:
                    try:
                        cpu_threads = int(line.split(":", 1)[1].strip())
                    except ValueError:
                        pass
    hw["cpu"] = cpu_model or "unknown"
    hw["cpu_threads"] = cpu_threads

    # RAM via /proc/meminfo.
    ram_gb: Optional[float] = None
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemTotal:"):
                    ram_kb = int(line.split()[1])
                    ram_gb = round(ram_kb / (1024.0 * 1024.0), 1)
                    break
    except (OSError, ValueError, IndexError):
        pass
    hw["ram_gb"] = ram_gb

    # OS.
    os_name = ""
    try:
        with open("/etc/os-release") as f:
            for line in f:
                if line.startswith("PRETTY_NAME="):
                    os_name = line.split("=", 1)[1].strip().strip('"')
                    break
    except OSError:
        pass
    hw["os"] = os_name or platform.platform()
    hw["arch"] = platform.machine()

    # CUDA toolkit presence.
    nvcc = shutil.which("nvcc")
    cuda_compiler = None
    if nvcc:
        rc, out, _ = _sh([nvcc, "--version"])
        if rc == 0:
            for line in out.splitlines():
                if "release" in line:
                    cuda_compiler = line.strip()
    hw["nvcc"] = nvcc
    hw["cuda_toolkit"] = cuda_compiler

    # Detect both shared-runtime links and the CUDA fatbin emitted by static
    # cudart builds. `ldd` alone is insufficient because CMake links cudart_static.
    solver_links_cuda: Optional[bool] = None
    if shutil.which("ldd"):
        rc, out, _ = _sh(["ldd", solver])
        if rc == 0:
            solver_links_cuda = any("cuda" in line.lower() for line in out.splitlines())
    hw["solver_links_cuda"] = solver_links_cuda
    solver_has_cuda_fatbin = False
    if shutil.which("readelf"):
        rc, out, _ = _sh(["readelf", "-S", solver])
        if rc == 0:
            solver_has_cuda_fatbin = ".nv_fatbin" in out or ".nvFatBinSegment" in out
    hw["solver_has_cuda_fatbin"] = solver_has_cuda_fatbin
    solver_has_cuda_support = bool(solver_links_cuda or solver_has_cuda_fatbin)

    if not gpus:
        hw["backend_note"] = (
            "no GPU detected; `--backend gpu` runs the CPU fallback of the GPU engine"
        )
    elif solver_has_cuda_support:
        hw["backend_note"] = (
            "GPU present and solver contains CUDA support; `--backend gpu` can execute "
            "on the device"
        )
    else:
        hw["backend_note"] = (
            "GPU present but the solver binary does not link a CUDA runtime "
            "(MARKOV_CERO_ENABLE_CUDA=OFF build); `--backend gpu` executes the GPU "
            "engine's host-fallback path, so gpu_* timings are NOT device measurements"
        )

    canon = "|".join([
        ",".join(g["name"] + "/" + g["memory_total"] + "/" + g["driver_version"]
                 for g in gpus),
        hw["cpu"], str(hw["cpu_threads"]), str(hw["ram_gb"]),
        hw["os"], hw["arch"], str(hw["cuda_toolkit"]),
        str(solver_links_cuda), str(solver_has_cuda_fatbin),
    ])
    hw["hardware_id"] = "hw-" + hashlib.sha1(canon.encode()).hexdigest()[:10]
    return hw


def print_hardware(hw: Dict[str, Any], sidecar: str) -> None:
    print(" Hardware:")
    if hw["gpus"]:
        for i, g in enumerate(hw["gpus"]):
            print(f"   GPU[{i}]:   {g['name']} | {g['memory_total']} | "
                  f"driver {g['driver_version']}")
    else:
        print(f"   GPU:      {hw['gpu_note']}")
    threads = f" ({hw['cpu_threads']} threads)" if hw["cpu_threads"] else ""
    print(f"   CPU:      {hw['cpu']}{threads}")
    ram = f"{hw['ram_gb']} GiB" if hw["ram_gb"] is not None else "unknown"
    print(f"   RAM:      {ram}")
    print(f"   OS:       {hw['os']} ({hw['arch']})")
    cuda = hw["cuda_toolkit"] or "nvcc not found (no CUDA toolkit)"
    linked = {True: "yes", False: "no", None: "unknown"}[hw["solver_links_cuda"]]
    print(f"   CUDA:     {cuda}; solver links CUDA runtime: {linked}")
    print(textwrap.fill(
        f"   Backend:  {hw['backend_note']}", width=TABLE_WIDTH,
        subsequent_indent="             "))
    print(f"   HW-ID:    {hw['hardware_id']}   recorded {hw['timestamp_utc']}")
    if sidecar:
        print(f"   Sidecar:  {sidecar}")


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


def main():
    parser = argparse.ArgumentParser(
        description="markov-cero GPU Benchmark Runner (Simplex vs CPU PDLP vs GPU PDLP)"
    )
    parser.add_argument("--solver", help="Path to markov-cero-solve executable")
    parser.add_argument(
        "--instances", nargs="+", default=DEFAULT_INSTANCES, help="Netlib instances to benchmark"
    )
    parser.add_argument("--data-dir", default="data/netlib", help="Directory containing MPS files")
    parser.add_argument(
        "--tolerance", type=float, default=1e-4, help="Relative KKT tolerance for PDLP"
    )
    parser.add_argument("--timeout", type=int, default=120, help="Per-run timeout in seconds")
    parser.add_argument(
        "--max-simplex-rows", type=int, default=0,
        help="Skip simplex baseline if rows exceed this value (0 = no limit)"
    )
    parser.add_argument(
        "--output", default="reports/gpu_benchmark.csv", help="Output path for benchmark CSV"
    )
    parser.add_argument(
        "--hardware-output", default="evidence/gpu_hardware.json",
        help="Path for the machine-readable hardware sidecar (empty string disables)"
    )

    args = parser.parse_args()

    solver = args.solver or find_solver_binary()
    if not solver or not os.access(solver, os.X_OK):
        print(f"Error: Solver executable not found: {solver}", file=sys.stderr)
        sys.exit(1)

    hardware = detect_hardware(solver)
    hw_id = hardware["hardware_id"]

    print("=" * TABLE_WIDTH)
    print(" markov-cero GPU Acceleration Benchmark (D-GPU-08 / Master Document §18.3)")
    print(f" Solver:    {solver}")
    print(f" Tolerance: {args.tolerance:.1e}")
    print(f" Output:    {args.output}")
    print(f" Instances: {', '.join(args.instances)}")
    print_hardware(hardware, args.hardware_output)
    print("=" * TABLE_WIDTH)

    header = (
        "{:<12} {:>5} {:>5} | {:>9} {:>6} | {:>8} {:>6} | "
        "{:>7} {:>7} {:>7} {:>8} | {:>7} | {:<44}"
    )
    print(
        header.format(
            "Instance", "Rows", "Cols",
            "Smplx(ms)", "Iters",
            "CPUp(ms)", "Iters",
            "H2D(ms)", "Krn(ms)", "D2H(ms)", "GPUp(ms)",
            "KrnSpd", "Result"
        )
    )
    print("-" * TABLE_WIDTH)

    records: List[Dict[str, Any]] = []
    all_passed = True

    for inst in args.instances:
        meta = BENCHMARKS.get(inst.lower(), {"rows": 0, "cols": 0, "optimal": 0.0})
        mps_file = os.path.join(args.data_dir, f"{inst.lower()}.mps")
        if not os.path.isfile(mps_file):
            alt = os.path.join("examples", f"{inst.lower()}.mps")
            if os.path.isfile(alt):
                mps_file = alt
            else:
                print(f"[!] Error: Cannot find {mps_file}; marking {inst.upper()} FAILED.",
                      file=sys.stderr)
                records.append(failed_record(
                    inst, meta["rows"], meta["cols"], meta["optimal"],
                    f"file=MPS not found: {mps_file}", hw_id))
                all_passed = False
                continue

        # 1. GPU PDLP
        res_gpu = run_configuration(
            solver, mps_file, engine="pdlp", backend="gpu",
            tolerance=args.tolerance, timeout=args.timeout
        )

        # 2. CPU PDLP
        res_cpu = run_configuration(
            solver, mps_file, engine="pdlp", backend="cpu",
            tolerance=args.tolerance, timeout=args.timeout
        )

        rows = res_gpu["rows"] or res_cpu["rows"] or meta["rows"]
        cols = res_gpu["cols"] or res_cpu["cols"] or meta["cols"]
        nnz = res_gpu["nonzeros"] or res_cpu["nonzeros"] or 0

        # 3. CPU Simplex baseline (optionally skip on massive instances)
        simplex_skipped = args.max_simplex_rows > 0 and rows > args.max_simplex_rows
        if simplex_skipped:
            res_simplex = {
                "exit_code": 0, "status": "Skipped", "verified": False,
                "rows": rows, "cols": cols, "nonzeros": nnz,
                "objective": float("nan"), "iterations": 0, "time_ms": float("nan"),
                "h2d_ms": 0.0, "kernel_ms": 0.0, "d2h_ms": 0.0, "total_ms": float("nan"),
                "error": f"Skipped (> {args.max_simplex_rows} rows)"
            }
        else:
            # Same pre-declared fallback policy as run_compare.py: try dual, then
            # primal, then pdlp. The reported time INCLUDES every failed attempt
            # (total time-to-verified-answer) — no cherry-picking. Rows where all
            # engines fail are reported as failures, never dropped.
            res_simplex = {"status": "NotRun", "verified": False, "time_ms": 0.0}
            for fallback_engine in ("dual", "primal", "pdlp"):
                attempt = run_configuration(
                    solver, mps_file, engine=fallback_engine,
                    tolerance=args.tolerance, timeout=args.timeout
                )
                attempt_time = attempt.get("time_ms") or 0.0
                attempt["time_ms"] = res_simplex["time_ms"] + attempt_time
                if attempt.get("status") == "Optimal" and attempt.get("verified"):
                    res_simplex = attempt
                    break
                res_simplex = attempt

        speedup_kernel = (
            (res_cpu["time_ms"] / res_gpu["kernel_ms"]) if res_gpu["kernel_ms"] > 0 else 0.0
        )
        speedup_end_to_end = (
            (res_cpu["time_ms"] / res_gpu["total_ms"]) if res_gpu["total_ms"] > 0 else 0.0
        )
        smplx_ms = res_simplex["time_ms"]
        speedup_vs_simplex = (
            (smplx_ms / res_gpu["total_ms"])
            if (res_gpu["total_ms"] > 0 and smplx_ms > 0 and not math.isnan(smplx_ms))
            else float("nan")
        )

        # Every configuration must terminate Optimal AND verify; a skipped simplex
        # baseline (by --max-simplex-rows) is explicitly not a failure. Any other
        # non-Optimal status (e.g. BLEND -> NumericalFailure) fails the instance and
        # must reach both the results table and the exit status.
        failures: List[str] = []
        failures += configuration_failures("simplex", res_simplex, allow_skipped=True)
        failures += configuration_failures("cpu_pdlp", res_cpu)
        failures += configuration_failures("gpu_pdlp", res_gpu)
        inst_passed = not failures
        if not inst_passed:
            all_passed = False

        smplx_str = f"{smplx_ms:.2f}" if not math.isnan(smplx_ms) else "N/A"
        if inst_passed:
            result_str = "PASS"
        else:
            result_str = "FAIL (" + "; ".join(failures) + ")"
        row = (
            "{:<12} {:>5} {:>5} | {:>9} {:>6} | {:>8.2f} {:>6} | "
            "{:>7.3f} {:>7.3f} {:>7.3f} {:>8.2f} | {:>6.2f}x | {}"
        ).format(
            inst.upper(), rows, cols,
            smplx_str, res_simplex["iterations"],
            res_cpu["time_ms"], res_cpu["iterations"],
            res_gpu["h2d_ms"], res_gpu["kernel_ms"], res_gpu["d2h_ms"], res_gpu["total_ms"],
            speedup_kernel, result_str
        )
        print(row)
        if not inst_passed:
            print(f"               [-] {inst.upper()} failed: {'; '.join(failures)}",
                  file=sys.stderr)

        ref_obj = meta["optimal"] if meta["optimal"] != 0.0 else res_gpu["objective"]
        record = {
            "instance": inst.upper(),
            "rows": rows,
            "cols": cols,
            "nonzeros": nnz,
            "reference_objective": ref_obj,
            "simplex_status": res_simplex["status"],
            "simplex_verified": res_simplex["verified"],
            "simplex_objective": res_simplex["objective"],
            "simplex_iterations": res_simplex["iterations"],
            "simplex_time_ms": res_simplex["time_ms"],
            "cpu_pdlp_status": res_cpu["status"],
            "cpu_pdlp_verified": res_cpu["verified"],
            "cpu_pdlp_objective": res_cpu["objective"],
            "cpu_pdlp_iterations": res_cpu["iterations"],
            "cpu_pdlp_time_ms": res_cpu["time_ms"],
            "gpu_pdlp_status": res_gpu["status"],
            "gpu_pdlp_objective": res_gpu["objective"],
            "gpu_pdlp_iterations": res_gpu["iterations"],
            "gpu_h2d_ms": res_gpu["h2d_ms"],
            "gpu_kernel_ms": res_gpu["kernel_ms"],
            "gpu_d2h_ms": res_gpu["d2h_ms"],
            "gpu_total_ms": res_gpu["total_ms"],
            "speedup_kernel": speedup_kernel,
            "speedup_end_to_end": speedup_end_to_end,
            "speedup_vs_simplex": speedup_vs_simplex,
            "gpu_verified": res_gpu["verified"],
            "failures": "; ".join(failures),
            "hardware_id": hw_id,
            "pass": inst_passed,
        }
        records.append(record)

    print("-" * TABLE_WIDTH)

    # Per-scale verdict table: every problem size is reported individually so a
    # regression on one scale cannot hide behind an aggregate.
    if records:
        print("Per-scale results (one row per problem size, no aggregation):")
        scale_header = (
            "{:<12} {:>6} | {:<17} {:<10} {:<10} | {:>6} | {:>9} | {}"
        )
        print(scale_header.format(
            "Instance", "Rows", "Simplex", "CPU-PDLP", "GPU-PDLP",
            "GPUver", "CPU/GPU", "Verdict"))
        for rec in records:
            verdict = "PASS" if rec["pass"] else f"FAIL: {rec['failures']}"
            gpuver = "yes" if rec["gpu_verified"] else "no"
            print(scale_header.format(
                rec["instance"], rec["rows"],
                str(rec["simplex_status"]), str(rec["cpu_pdlp_status"]),
                str(rec["gpu_pdlp_status"]), gpuver,
                fmt_speedup(rec["speedup_end_to_end"]), verdict))
        passed = sum(1 for r in records if r["pass"])
        print(f" Summary: {passed}/{len(records)} instance(s) passed, "
              f"{len(records) - passed} failed.")

    if records:
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        with open(args.output, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=RECORD_FIELDS)
            writer.writeheader()
            writer.writerows(records)
        print(f"\n[+] Results successfully written to {args.output}")

    if args.hardware_output:
        os.makedirs(os.path.dirname(os.path.abspath(args.hardware_output)), exist_ok=True)
        sidecar = dict(hardware)
        sidecar["run"] = {
            "solver": solver,
            "cwd": os.getcwd(),
            "instances": args.instances,
            "tolerance": args.tolerance,
            "timeout_s": args.timeout,
            "max_simplex_rows": args.max_simplex_rows,
            "output_csv": args.output,
        }
        sidecar["results"] = [
            {"instance": r["instance"], "pass": r["pass"], "failures": r["failures"]}
            for r in records
        ]
        with open(args.hardware_output, "w") as f:
            json.dump(sidecar, f, indent=2)
            f.write("\n")
        print(f"[+] Hardware sidecar written to {args.hardware_output}")

    if not records:
        print("[-] No results recorded: no selected instance produced output.",
              file=sys.stderr)
        sys.exit(1)

    if all_passed:
        print("[+] All selected GPU benchmark problems converged and verified.")
        sys.exit(0)
    else:
        failed = [r for r in records if not r["pass"]]
        print("[-] Failed instances:", file=sys.stderr)
        for r in failed:
            print(f"    {r['instance']}: {r['failures']}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
