from .run_gpu_config import (
    Any, Dict, List, Optional, TABLE_WIDTH, datetime, hashlib, os, platform, shutil, subprocess, textwrap
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
