from __future__ import annotations
from .run_full_compare_config import (
    AGREE_TOL, GLPK_NOTE, REPO, SOLVER_ORDER, csv, json, math, os, sys
)
from .run_full_compare_run_glpk import _probe
from .run_full_compare_run_highs import find_glpsol
from .run_full_compare_run_glpk import pulp_version
from .run_full_compare_run_highs import rel_diff

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
