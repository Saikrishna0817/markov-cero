from .run_compare_config import (
    REPO_ROOT, hashlib, json, math, os, subprocess, sys, time
)

def repo_path(*parts):
    return os.path.join(REPO_ROOT, *parts)

def load_instance_list(suites):
    """Pre-registered shared instance set (data/compare/<suite>.txt: name<TAB>mps-path)."""
    instances = []
    seen = set()
    for suite in suites:
        path = repo_path("data", "compare", f"{suite}.txt")
        if not os.path.isfile(path):
            print(f"[!] missing instance list {path}, skipping suite '{suite}'", file=sys.stderr)
            continue
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                name, _, mps = line.partition("\t")
                name, mps = name.strip(), mps.strip()
                key = (suite, name)
                if key in seen:
                    continue
                seen.add(key)
                instances.append({"suite": suite, "name": name, "mps": repo_path(mps)})
    return instances

def sha256_file(path, full=False):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    digest = h.hexdigest()
    return digest if full else digest[:16]

def find_solver(arg):
    if arg:
        return arg
    for cand in (repo_path("build_w5/markov-cero-solve"),
                 repo_path("build/markov-cero-solve"),
                 repo_path("build-rw2/markov-cero-solve"),
                 repo_path("bin/markov-cero-solve")):
        if os.path.isfile(cand) and os.access(cand, os.X_OK):
            return cand
    return None

def run_markov_cero(solver, mps, timeout, threads):
    """One automatic-engine process per sample, including startup and output."""
    engines = [None]
    total_ms = 0.0
    last = None
    for engine in engines:
        cmd = [solver, mps, "--time-limit", str(timeout)]
        cmd += ["--threads", str(threads)]
        if engine:
            cmd += ["--engine", engine]
        t0 = time.perf_counter()
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True,
                                  timeout=timeout + 10)
        except subprocess.TimeoutExpired:
            total_ms += timeout * 1000.0
            last = {"status": "Timeout", "objective": float("nan"),
                    "time_ms": total_ms, "verified": False, "engine": engine or "auto"}
            continue
        total_ms += (time.perf_counter() - t0) * 1000.0
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
            last = {"status": "ParseError", "objective": float("nan"),
                    "time_ms": total_ms, "verified": False, "engine": engine or "auto"}
            continue
        last = {"status": parsed.get("status", "Unknown"),
                "objective": parsed.get("objective", float("nan")),
                "time_ms": total_ms,
                "verified": bool(parsed.get("verified", False)),
                "engine": parsed.get("resolved_engine", engine or "auto")}
        if last["status"] == "Optimal" and last["verified"]:
            break
    return last

def run_highs(highs_mod, mps, timeout, default_penalty_ms, threads=1, python_executable=None):
    from .oracle_process import run_highs_process
    result = run_highs_process(mps, timeout, threads, python_executable)
    return {"status": result["status"], "objective": result.get("objective", float("nan")),
            "time_ms": result["runtime_ms"]}


def geometric_mean_ratio(ratios, penalty):
    """GM of runtime ratios; unsolved baseline runs get the penalty time."""
    logs = [math.log(max(r, 1e-9)) for r in ratios if not math.isnan(r)]
    if not logs:
        return float("nan")
    return math.exp(sum(logs) / len(logs))

def dolan_more_profile(results, tau_max=8.0, points=32):
    """rho_s(tau): fraction of instances solved within tau x best time."""
    taus = [tau_max * (i + 1) / points for i in range(points)]
    curve = []
    for tau in taus:
        within = 0
        total = 0
        for row in results:
            if math.isnan(row["mc_time_ms"]) or math.isnan(row["hi_time_ms"]):
                continue
            total += 1
            best = min(row["mc_time_ms"], row["hi_time_ms"])
            if best > 0 and row["mc_time_ms"] <= tau * best:
                within += 1
        curve.append((tau, within / total if total else 0.0))
    return curve

def write_profile_svg(curve, path):
    width, height = 480, 320
    pad = 40
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width//2}" y="18" text-anchor="middle" font-size="14">'
        'Dolan-More performance profile (markov-cero)</text>',
    ]
    n = len(curve)
    if n:
        pts = " ".join(
            f"{pad + (width-2*pad)*i/(n-1):.1f},"
            f"{height-pad-(height-2*pad)*tau_v:.1f}"
            for i, (_, tau_v) in enumerate(curve)
        )
        lines.append(f'<polyline points="{pts}" fill="none" stroke="#0868ac" stroke-width="2"/>')
        for frac in (0.5, 0.75, 1.0):
            y = height - pad - (height - 2 * pad) * frac
            lines.append(f'<line x1="{pad}" y1="{y:.1f}" x2="{width-pad}" y2="{y:.1f}" '
                         f'stroke="#ddd" stroke-dasharray="4,3"/>')
    lines.append(f'<line x1="{pad}" y1="{height-pad}" x2="{width-pad}" y2="{height-pad}" '
                 'stroke="black"/>')
    lines.append(f'<line x1="{pad}" y1="{pad}" x2="{pad}" y2="{height-pad}" stroke="black"/>')
    lines.append(f'<text x="{width//2}" y="{height-8}" text-anchor="middle" font-size="11">'
                 'tau (runtime ratio vs best solver)</text>')
    lines.append(f'<text x="12" y="{height//2}" font-size="11" '
                 'transform="rotate(-90 12 '
                 f'{height//2})">fraction solved</text>')
    lines.append("</svg>")
    with open(path, "w") as f:
        f.write("\n".join(lines))
