from __future__ import annotations
from .run_full_compare_config import (
    AGREE_TOL, GLPK_NOTE, REPO, SOLVER_ORDER, csv, hashlib, json, math, os,
    sys, tempfile
)
from .frozen_timing_config import FROZEN_ON
from .run_full_compare_run_glpk import _probe
from .run_full_compare_run_highs import find_glpsol
from .run_full_compare_run_glpk import pulp_version
from .run_full_compare_run_highs import rel_diff

def _probe_markov_version(binary: str) -> tuple:
    """CLI version of the markov-cero binary from its solve JSON / banner."""
    model = ("NAME PROBE\nROWS\n N COST\n L ONE1\nCOLUMNS\n"
             " X COST 1\n X ONE1 1\nRHS\n RHS ONE1 1\nBOUNDS\n UP B X 1\nENDATA\n")
    handle, path = tempfile.mkstemp(prefix="markov_probe_", suffix=".mps")
    try:
        with os.fdopen(handle, "w") as stream:
            stream.write(model)
        rc, out = _probe([binary, path], timeout=60)
        if rc != 0:
            return "", out[-200:]
        for line in reversed(out.splitlines()):
            line = line.strip()
            if line.startswith("{") and line.endswith("}"):
                try:
                    payload = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if payload.get("version"):
                    return str(payload["version"]), ""
        return "", "solve JSON carried no version field"
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass


def probe_availability(binary: str) -> dict:
    probes = {}

    def add(name, available, version, how, notes="", command="", source=""):
        probes[name] = {"available": bool(available), "version": version or "unknown",
                        "how": how, "probe_command": command,
                        "version_source": source, "notes": notes}

    if binary and os.path.isfile(binary) and os.access(binary, os.X_OK):
        rc, _ = _probe([binary, "--help"])
        version, version_notes = _probe_markov_version(binary)
        add("markov-cero", rc == 0, version,
            "binary `--help` probe; version from the solve JSON of a one-row probe model",
            version_notes or "engine selection: auto; CLI solver",
            command=f"{binary} --help; {binary} <probe.mps> -> JSON 'version'",
            source="CLI")
    else:
        add("markov-cero", False, "", "binary probe",
            f"binary not found or not executable: {binary}",
            command=f"{binary} --help", source="CLI")

    highs_probe = ["import highspy; print(highspy.Highs().version())"]
    rc, out = _probe([sys.executable, "-c", highs_probe[0]])
    add("HiGHS", rc == 0, out if rc == 0 else "", "import highspy (in-process API)",
        "" if rc == 0 else out[-200:],
        command=f'{sys.executable} -c "{highs_probe[0]}"', source="python import")

    cbc_probe = ("import pulp; from pulp import PULP_CBC_CMD; "
                 "print(PULP_CBC_CMD(msg=0).path)")
    rc, out = _probe([sys.executable, "-c", cbc_probe])
    cbc_notes = ""
    cbc_version = "unknown"
    cbc_command = f'{sys.executable} -c "{cbc_probe}"'
    if rc == 0 and out and os.path.isfile(out.splitlines()[-1]):
        cbc_path = out.splitlines()[-1]
        brc, banner = _probe([cbc_path], timeout=20)
        for line in banner.splitlines():
            if line.strip().startswith("Version:"):
                cbc_version = line.split(":", 1)[1].strip()
        cbc_command = f"{cbc_path}   # reached via {cbc_probe}"
        add("CBC", True, cbc_version, "pulp PULP_CBC_CMD (bundled CBC binary)",
            f"pulp {pulp_version()} + CBC {cbc_version}",
            command=cbc_command, source="CLI")
    else:
        add("CBC", False, "", "pulp PULP_CBC_CMD",
            (out or "pulp not importable")[-200:],
            command=cbc_command, source="python import")

    scip_probe = ("import pyscipopt; from pyscipopt import Model; m=Model();"
                  "print('%s (PySCIPOpt %s)' % ("
                  "'.'.join(str(x) for x in (m.getMajorVersion(), m.getMinorVersion(),"
                  "m.getTechVersion())), pyscipopt.__version__))")
    rc, out = _probe([sys.executable, "-c", scip_probe])
    add("SCIP", rc == 0, out if rc == 0 else "", "import pyscipopt (in-process API)",
        "" if rc == 0 else out[-200:],
        command=f'{sys.executable} -c "{scip_probe}"', source="python import")

    glpsol = find_glpsol()
    if glpsol:
        rc, out = _probe([glpsol, "--version"])
        version = out.splitlines()[0].strip() if rc == 0 and out else "unknown"
        add("GLPK", rc == 0, version, glpsol,
            "external glpsol process; GLPK uses one thread",
            command=f"{glpsol} --version", source="CLI")
    else:
        add("GLPK", False, "", "project-local .venv/bin or PATH", GLPK_NOTE,
            command="glpsol --version", source="CLI")
    return probes


def write_comparator_manifest(path, probes, binary) -> str:
    """Freeze comparator versions/availability (backlog item 7)."""
    payload = {
        "date": FROZEN_ON,
        "frozen_by": "scripts/support/run_full_compare_annotate.py probe_availability",
        "comparator_order": list(SOLVER_ORDER),
        "probe_python": sys.executable,
        "probe_python_version": sys.version.split()[0],
        "solver_binary": {
            "path": binary,
            "sha256": _sha256_file(binary),
            "executable": bool(binary and os.path.isfile(binary)
                               and os.access(binary, os.X_OK)),
        },
        "comparators": [
            {
                "name": name,
                "kind": "markov-cero" if name == "markov-cero" else "external",
                "available": bool(probes.get(name, {}).get("available", False)),
                "version": probes.get(name, {}).get("version", "unknown"),
                "version_source": probes.get(name, {}).get("version_source", ""),
                "probe_command": probes.get(name, {}).get("probe_command", ""),
                "method": probes.get(name, {}).get("how", ""),
                "notes": probes.get(name, {}).get("notes", ""),
            }
            for name in list(SOLVER_ORDER)
        ],
        "unavailable": [name for name in SOLVER_ORDER
                        if not probes.get(name, {}).get("available", False)],
        "limitations": [
            "Probe results are point-in-time for this host and .venv/.compare-venv; "
            "an unavailable comparator stays in the manifest with its probe command.",
            GLPK_NOTE if not probes.get("GLPK", {}).get("available") else
            "glpsol runs as an external single-threaded process.",
        ],
    }
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as handle:
        json.dump(payload, handle, indent=2, sort_keys=False)
        handle.write("\n")
    return path


def _sha256_file(path) -> str:
    if not path or not os.path.isfile(path):
        return ""
    with open(path, "rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


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
