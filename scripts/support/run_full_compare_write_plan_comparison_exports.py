from __future__ import annotations
from .run_full_compare_config import (
    REPO, SCRIPTS, TABLES, csv, json, os, subprocess, sys, tempfile
)
from .run_full_compare_run_highs import rel_diff
from .run_full_compare_annotate import write_results_csv

def write_plan_comparison_exports(out_dir, rows, instances, refs):
    """Write the five W9 suite tables and separate LP/MILP profiles.

    Published Gurobi rows are not fabricated here: the local Mittelmann source
    table records heterogeneous published runtime fields, so that ratio stays
    blank until an exact solver/instance/time match can be established.
    """
    instance_meta = {name: (suite, cls) for suite, cls, name in instances}
    timings = {(r["instance"], r["solver"]): r for r in rows}
    groups = {
        "netlib_lp_comparison.csv": lambda s, c: s == "netlib" and c == "LP",
        "miplib_comparison.csv": lambda s, c: s == "miplib" and c == "MILP",
        "mittelmann_lp_comparison.csv": lambda s, c: s == "mittelmann" and c == "LP",
        "mittelmann_milp_comparison.csv": lambda s, c: s == "mittelmann" and c == "MILP",
        "qplib_comparison.csv": lambda s, c: s == "qp" and c == "QP",
    }
    fields = ["solver", "instance", "status", "objective", "obj_error_vs_best",
              "time_ms", "ratio_vs_highs", "ratio_vs_gurobi", "pass"]
    os.makedirs(out_dir, exist_ok=True)
    for filename, include in groups.items():
        path = os.path.join(out_dir, filename)
        with open(path, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            for row in rows:
                suite, cls = instance_meta.get(row["instance"], ("", ""))
                if not include(suite, cls):
                    continue
                inst = row["instance"]
                obj = row.get("objective", "")
                known = refs.get(inst)
                if known is None:
                    mc = timings.get((inst, "markov-cero"), {})
                    if mc.get("status") == "Optimal" and mc.get("objective") != "":
                        known = float(mc["objective"])
                err = ""
                if obj not in (None, "") and known is not None:
                    err = rel_diff(float(obj), float(known))
                highs = timings.get((inst, "HiGHS"), {})
                ratio = ""
                if (row.get("status") == "Optimal" and highs.get("status") == "Optimal"
                        and float(highs.get("runtime_ms", 0.0)) > 0):
                    ratio = float(row["runtime_ms"]) / float(highs["runtime_ms"])
                writer.writerow({
                    "solver": row["solver"], "instance": inst,
                    "status": row.get("status", ""), "objective": obj,
                    "obj_error_vs_best": err, "time_ms": row.get("runtime_ms", ""),
                    "ratio_vs_highs": ratio, "ratio_vs_gurobi": "",
                    "pass": bool(row.get("status") == "Optimal" and row.get("verified")),
                })

    # Use the same profile implementation and denominator rules as the overall
    # profile; only the problem set changes by category.
    for cls, instances_in_class, suffix in (
        ("LP", {name for _, kind, name in instances if kind == "LP"}, "lp"),
        ("MILP", {name for _, kind, name in instances if kind == "MILP"}, "milp"),
    ):
        filtered = [r for r in rows if r["instance"] in instances_in_class]
        if not filtered:
            continue
        fd, temp_csv = tempfile.mkstemp(prefix=f"markov-{suffix}-", suffix=".csv")
        os.close(fd)
        try:
            write_results_csv(temp_csv, filtered)
            subprocess.run(
                [sys.executable, os.path.join(SCRIPTS, "dolan_more_profile.py"),
                 "--input", temp_csv,
                 "--svg", os.path.join(out_dir, f"dolan_more_{suffix}.svg"),
                 "--csv", os.path.join(out_dir, f"dolan_more_{suffix}_profile_data.csv")],
                check=True, capture_output=True, text=True, cwd=REPO)
        finally:
            os.unlink(temp_csv)
    return groups

def write_mittelmann_reference(instances) -> str:
    lines = ["# Mittelmann published-table reference (D-09)", ""]
    res_csv = os.path.join(TABLES, "milp_12threads.csv")
    published = {}
    if os.path.isfile(res_csv):
        with open(res_csv) as f:
            table_rows = list(csv.DictReader(f))
        solvers = [c for c in table_rows[0] if c not in ("instance", "url", "accessed")]
        lines += [f"Source: {table_rows[0]['url']} (accessed {table_rows[0]['accessed']})",
                  f"Instances: {len(table_rows)} (MIPLIB2017 benchmark, preprocessed)",
                  "",
                  "| solver | solved/total |",
                  "|---|---|"]
        solved = {s: sum(1 for r in table_rows
                         if r[s].strip().lower() not in ("timeout", "failed", ""))
                  for s in solvers}
        for s in solvers:
            lines.append(f"| {s} | {solved[s]}/{len(table_rows)} |")
        lines += ["", "markov-cero does not appear in these published tables; "
                      "the local open-source comparisons (results CSVs above) are "
                      "measured baselines. Published numbers provide the "
                      "commercial-solver context without licenses."]
        for row in table_rows:
            published[row["instance"].strip()] = row
    else:
        lines.append("No scraped tables found; run scripts/scrape_mittelmann.py.")

    lines += ["", "## Per-instance published entries (curated data/mittelmann set, M6-37)",
              "",
              "Instance names are matched exactly against "
              "`data/mittelmann_tables/milp_12threads.csv` after stripping the "
              "published `p_` prefix.  The published tables record **wall-clock "
              "times only** - they carry no objective values, so every objective "
              "entry below is marked unverified unless a provenance file supplies "
              "a `reference_objective`.", "",
              "| instance | published entry | published times (s) | reference objective | verified |",
              "|---|---|---|---|---|"]
    for suite, _, name in instances:
        if suite != "mittelmann":
            continue
        entry = published.get(f"p_{name}")
        ref_objective = None
        prov = os.path.join(REPO, "data", suite, f"{name}.provenance.json")
        if os.path.isfile(prov):
            try:
                with open(prov) as f:
                    ref_objective = json.load(f).get("reference_objective")
            except (OSError, json.JSONDecodeError):
                ref_objective = None
        if entry:
            times = ", ".join(f"{s} {entry[s]}" for s in
                              [c for c in entry if c not in
                               ("instance", "url", "accessed")])
            entry_text = f"p_{name} (milp_12threads.csv)"
        else:
            times = "-"
            entry_text = "no exact name match"
        if ref_objective is not None:
            obj_text = repr(ref_objective)
            verified = "yes (provenance reference_objective)"
        else:
            obj_text = "-"
            verified = "unverified (no published/provenance objective)"
        lines.append(f"| {name} | {entry_text} | {times} | {obj_text} | {verified} |")
    return "\n".join(lines) + "\n"

def parse_mittelmann_reference(path: str) -> dict:
    entries = {}
    if not os.path.isfile(path):
        return entries
    with open(path) as f:
        lines = f.read().splitlines()
    header = None
    for idx, line in enumerate(lines):
        if line.startswith("| instance | published entry |"):
            header = [c.strip() for c in line.strip("|").split("|")]
            for row in lines[idx + 2:]:
                if not row.startswith("|"):
                    break
                cells = [c.strip() for c in row.strip("|").split("|")]
                if len(cells) >= len(header):
                    entries[cells[0]] = dict(zip(header, cells))
            break
    return entries

def fmt(value) -> str:
    if value in (None, ""):
        return "-"
    try:
        return f"{float(value):.6g}"
    except (TypeError, ValueError):
        return str(value)
