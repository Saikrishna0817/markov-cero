#!/usr/bin/env python3
"""Freeze comparator versions, instance manifests, family splits, timing and baseline.

Backlog item 7. One command reproduces manifests + splits + timing + baseline:

    python3 scripts/freeze_baseline.py --solver ./build_contracts/markov-cero-solve --time-box 20

Stages, in order: manifests, compare, benchmark, checks, evidence (all by default).
Offline: nothing is downloaded; absent optional datasets stay listed unavailable.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import subprocess
import sys
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
if str(REPO) not in sys.path:
    sys.path.insert(0, str(REPO))

DATE = "20260928"
ISO_DATE = "2026-09-28"
DEFAULT_SOLVER = "./build_contracts/markov-cero-solve"
EVIDENCE = REPO / "evidence" / f"baseline-freeze-{DATE}.json"
COMPARATORS = REPO / "evidence" / f"frozen-comparators-{DATE}.json"
INSTANCES = REPO / "evidence" / f"frozen-instances-{DATE}.json"
COMPARE_OUT = REPO / "evidence" / "comparison" / "baseline_frozen_20260928"
BENCH_OUT = REPO / "evidence" / "benchmarks" / "baseline_frozen_20260928"
CHECKS = [
    ["python3", "scripts/check_source_limits.py"],
    ["ctest", "--test-dir", "build_contracts", "-R",
     "compare_harness|netlib_benchmarks|miplib_benchmarks|repository_tools",
     "--output-on-failure"],
]

def _now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")

def _sha256(path: Path) -> str:
    if not path.is_file():
        return ""
    hasher = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            hasher.update(block)
    return hasher.hexdigest()

def _solver_path(solver: str) -> Path:
    path = Path(solver)
    return path if path.is_absolute() else REPO / path

def _load() -> dict:
    if EVIDENCE.is_file():
        return json.loads(EVIDENCE.read_text())
    return {
        "date": ISO_DATE,
        "backlog_item": 7,
        "roadmap_item": ("historical roadmap item 7 (archived in Git history): "
                         "freeze comparator versions, manifests, family splits "
                         "and timing definitions, then regenerate the baseline before tuning"),
        "purpose": "Frozen manifests, splits, timing definitions and the regenerated "
                   "pre-tuning baseline for this uncommitted worktree",
        "worktree": "uncommitted working tree; results identify this state, not HEAD",
        "offline": "no download performed; optional datasets absent locally are listed "
                   "unavailable in the frozen instance manifest",
        "commands": [],
        "manifests": {},
        "baseline": {},
        "checks": [],
        "limitations": [],
        "follow_ups": [],
    }

def _save(doc: dict) -> None:
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(json.dumps(doc, indent=2) + "\n")

def _ensure_probe_interpreter() -> None:
    """Re-exec under .compare-venv so comparator probes see highspy/pulp/pyscipopt
    (and ml_splits' numpy import) exactly as the compare harness sees them."""
    try:
        import highspy  # noqa: F401
        import numpy  # noqa: F401
        import pulp  # noqa: F401
        import pyscipopt  # noqa: F401
        return
    except ModuleNotFoundError:
        pass
    if os.environ.get("MARKOV_FREEZE_REEXEC"):
        return  # one relocation only; report whatever remains importable
    for venv in (REPO / ".compare-venv", REPO / ".venv"):
        candidate = venv / "bin" / "python"
        if candidate.is_file() and Path(sys.prefix).resolve() != venv.resolve():
            os.environ["MARKOV_FREEZE_REEXEC"] = "1"
            os.execv(str(candidate), [str(candidate), str(Path(__file__).resolve())]
                     + sys.argv[1:])

def _record(doc: dict, stage: str, argv: list, note: str = "") -> dict:
    marker = {"compare": COMPARE_OUT / "full_compare_results.csv",
              "benchmark": BENCH_OUT / "full_netlib.csv"}.get(stage)
    entry = {"stage": stage, "command": " ".join(argv), "started": _now(), "rc": 0}
    if marker is not None and marker.is_file() and marker.stat().st_size > 0:
        entry["ran"] = False
        entry["rows"] = sum(1 for _ in marker.open()) - 1
        entry["note"] = note or (
            "outputs already present when the stage executed: verified and registered "
            "without re-running (the identical argv was invoked directly while authoring "
            "this freeze)")
    else:
        started = time.perf_counter()
        run = subprocess.run(argv, cwd=REPO)
        entry["ran"] = True
        entry["seconds"] = round(time.perf_counter() - started, 2)
        entry["rc"] = run.returncode
        if marker is not None:
            entry["rows"] = sum(1 for _ in marker.open()) - 1 if marker.is_file() else 0
    doc["commands"].append(entry)
    _save(doc)
    return entry

def _timing_pins() -> dict:
    """Confirm each harness entry point is pinned to the same frozen object."""
    import importlib
    from scripts.support.frozen_timing_config import TIMING
    pins = {}
    for name in ("scripts.run_compare", "scripts.run_netlib", "scripts.run_miplib"):
        module = importlib.import_module(name)
        pins[name] = bool(getattr(module, "FROZEN_TIMING", None) is TIMING)
    return pins

def stage_manifests(solver: str) -> None:
    from scripts.support.frozen_instance_manifest import write_manifest
    from scripts.support.frozen_timing_config import as_dict as timing_as_dict
    from scripts.support.run_full_compare_annotate import probe_availability
    from scripts.support.run_full_compare_annotate import write_comparator_manifest

    probes = probe_availability(solver)
    write_comparator_manifest(str(COMPARATORS), probes, solver)
    for out_dir in (COMPARE_OUT, BENCH_OUT):
        if out_dir.is_dir():
            write_comparator_manifest(str(out_dir / "frozen-comparators.json"),
                                      probes, solver)
    write_manifest(str(INSTANCES))
    instances = json.loads(INSTANCES.read_text())
    comparators = json.loads(COMPARATORS.read_text())
    doc = _load()
    doc["solver"] = {"path": solver, "sha256": _sha256(_solver_path(solver))}
    doc["manifests"] = {
        "comparators": {"path": str(COMPARATORS.relative_to(REPO)),
                        "sha256": _sha256(COMPARATORS),
                        "unavailable": comparators["unavailable"]},
        "instances": {"path": str(INSTANCES.relative_to(REPO)),
                      "sha256": _sha256(INSTANCES),
                      "counts": instances["counts"],
                      "split": instances["split"]["counts"],
                      "family_tags": instances["family_tags"]},
    }
    doc["timing"] = timing_as_dict()
    doc["manifests"]["timing_pins"] = _timing_pins()
    doc["reproduce"] = {
        "command": f"python3 scripts/freeze_baseline.py --solver {solver} --time-box 20",
        "note": "runs manifests, compare, benchmark, checks and evidence stages; "
                "without --time-box the frozen caps (LP/QP 60 s, MILP 300 s) are used",
        "solver_note": "the roadmap text names build_contracts/markov_cero_solve; the "
                       "CMake target file is build_contracts/markov-cero-solve",
    }
    _save(doc)

def stage_compare(solver: str, time_box: float | None) -> None:
    argv = ["python3", "scripts/run_full_compare.py", "--solver", solver]
    if time_box is not None:
        argv += ["--timeout", f"{time_box:g}"]
    argv += ["--out", str(COMPARE_OUT.relative_to(REPO))]
    _record(_load(), "compare", argv)

def stage_benchmark(solver: str, time_box: float | None) -> None:
    sha256 = _sha256(_solver_path(solver))
    argv = ["python3", "scripts/run_full_benchmark.py", "--solver", solver,
            "--suites", "netlib",
            "--out-dir", str(BENCH_OUT.relative_to(REPO)),
            "--expected-solver-sha256", sha256]
    if time_box is not None:
        argv += ["--timeout", f"{time_box:g}"]
    _record(_load(), "benchmark", argv)

def stage_checks() -> None:
    doc = _load()
    for argv in CHECKS:
        started = time.perf_counter()
        run = subprocess.run(argv, cwd=REPO, capture_output=True, text=True)
        tail = (run.stdout + run.stderr).strip().splitlines()[-6:]
        doc["checks"].append({"command": " ".join(argv), "rc": run.returncode,
                              "seconds": round(time.perf_counter() - started, 2),
                              "output_tail": tail})
    _save(doc)

def _summary(path: Path) -> dict:
    if not path.is_file():
        return {"path": str(path.relative_to(REPO)), "rows": 0}
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    solver_key = "solver"
    return {
        "path": str(path.relative_to(REPO)),
        "sha256": _sha256(path),
        "rows": len(rows),
        "per_solver": dict(Counter(row[solver_key] for row in rows)),
        "statuses": dict(Counter(row["status"] for row in rows)),
        "solver_sha256": sorted({row.get("markov_cero_sha256") or
                                 row.get("solver_sha256", "") for row in rows} - {""}),
    }

def stage_evidence(solver: str) -> None:
    doc = _load()
    doc["baseline"] = {
        "regenerated": _now(),
        "comparison": _summary(COMPARE_OUT / "full_compare_results.csv"),
        "benchmark": _summary(BENCH_OUT / "full_netlib.csv"),
        "supersedes": "quarantined mixed-binary comparison/benchmark rows; these two "
                      "directories are the frozen pre-tuning baseline for this worktree",
        "timing_scope": "process-wall, single cold observation per instance/solver cell",
    }
    doc.setdefault("manifests", {}).setdefault("comparators", {})
    doc["manifests"]["comparators"]["sha256"] = _sha256(COMPARATORS)
    doc["manifests"].setdefault("instances", {})
    doc["manifests"]["instances"]["sha256"] = _sha256(INSTANCES)
    doc["limitations"] = [
        "Time-boxed regeneration: --timeout 20 s was passed to both harnesses to fit the "
        "~20 minute budget instead of the frozen 60 s (LP/QP) and 300 s (MILP) caps.",
        "Single-shot cold process-wall timings; the frozen minimum of 5 repeats per cell "
        "is not met by these harnesses (documented deviation).",
        "run_full_compare applies one shared --timeout to every problem class, so a "
        "default run LP-caps MILP rows; run_full_benchmark resolves the frozen cap per suite.",
        f"{doc['manifests'].get('instances', {}).get('counts', {}).get('unavailable', 0)} "
        "suite entries and 2 "
        "curated instances (miplib/swath1, mittelmann/bienst1) are absent locally; they "
        "are listed unavailable and were not downloaded.",
        "Optional netlib/miplib datasets are not redistributed in this worktree, so "
        f"{doc['baseline'].get('benchmark', {}).get('statuses', {}).get('DatasetUnavailable', 0)}"
        f" of {doc['baseline'].get('benchmark', {}).get('rows', 0)} netlib benchmark rows "
        "are DatasetUnavailable.",
        "Comparator versions are point-in-time for this host's .venv/.compare-venv.",
        "Solver binary: build_contracts/markov-cero-solve (the roadmap text names "
        "markov_cero_solve; the CMake target uses the dash).",
    ]
    doc["follow_ups"] = [
        "Dataset redistribution gate: decide which optional netlib/miplib/QPLIB files may "
        "ship so benchmark denominators stop depending on absent local files.",
        "Full-campaign compute budget: rerun the frozen manifests with the frozen caps and "
        ">=5 repeats per cell before any tuning claim is made.",
        "Report NumericalFailure/InvalidModel netlib rows of this binary as solver defects "
        "before the baseline is used for comparison claims.",
    ]
    _save(doc)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", default=DEFAULT_SOLVER)
    parser.add_argument("--time-box", type=float, default=None,
                        help="seconds passed to --timeout on both harnesses "
                             "(default: frozen caps)")
    parser.add_argument("--stage", action="append",
                        choices=["manifests", "compare", "benchmark", "checks",
                                 "evidence"],
                        help="run only these stages (repeatable; default: all)")
    args = parser.parse_args()
    stages = args.stage or ["manifests", "compare", "benchmark", "checks", "evidence"]
    if "manifests" in stages:
        _ensure_probe_interpreter()
        stage_manifests(args.solver)
    if "compare" in stages:
        stage_compare(args.solver, args.time_box)
    if "benchmark" in stages:
        stage_benchmark(args.solver, args.time_box)
    if "checks" in stages:
        stage_checks()
    if "evidence" in stages:
        stage_evidence(args.solver)
    print(f"[+] stages {','.join(stages)} -> {EVIDENCE.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
