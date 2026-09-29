#!/usr/bin/env python3
"""Frozen instance manifest and family splits (backlog item 7).

Records every instance the two harnesses can run — the curated comparison set
(`run_full_compare_config.CURATED`) and the benchmark suites
(`run_full_benchmark_config.SUITES`) — with a family tag, source path or
optional-dataset identity, local SHA-256 when the file is present, and a
deterministic 70/15/15 train/tune/holdout split assigned by the seeded hash in
`scripts/ml/ml_splits.py` (freeze hook `frozen_split`).

Offline by design: nothing is downloaded; absent optional datasets are listed
as unavailable.
"""
from __future__ import annotations

import hashlib
import json
import os

from .frozen_timing_config import FROZEN_ON, as_dict as timing_as_dict
from .run_full_compare_config import CURATED, REPO
from .run_full_benchmark_config import SUITES

FAMILY_OF_SUITE = {
    "netlib": "netlib",
    "miplib": "miplib",
    "mittelmann": "miplib",
    "qp": "qp",
    "cases": "domain",
    "refinery": "domain",
}

FAMILY_TAGS = {
    "netlib": "NETLIB continuous LP collection",
    "miplib": "MIPLIB and the Mittelmann MIP extension of the same family",
    "qp": "quadratic objectives (QPLIB)",
    "domain": "in-house domain cases (cases/refinery)",
}


def _sha256(path: str) -> str:
    if not os.path.isfile(path):
        return ""
    hasher = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            hasher.update(block)
    return hasher.hexdigest()


def _optional_index() -> dict:
    manifest = os.path.join(REPO, "data", "optional-datasets.json")
    if not os.path.isfile(manifest):
        return {}
    with open(manifest) as handle:
        entries = json.load(handle)
    return {entry["path"]: entry for entry in entries}


def _source_fields(relative_path: str, optional: dict) -> dict:
    entry = optional.get(relative_path)
    fields = {"kind": "optional" if entry else "local",
              "path": relative_path}
    if entry:
        fields["dataset_id"] = f"{entry.get('family', '')}:{entry['sha256']}"
        fields["dataset_sha256"] = entry["sha256"]
        fields["source_revision"] = entry.get("source_revision", "")
        fields["source_url"] = entry.get("source_url", "")
    return fields


def _entry(relative_path: str, family: str, suite: str, optional: dict,
           problem_class: str = "") -> dict:
    absolute = os.path.join(REPO, relative_path)
    present = os.path.isfile(absolute)
    record = {
        "id": relative_path,
        "name": os.path.splitext(os.path.basename(relative_path))[0],
        "family": family,
        "suite": suite,
        "problem_class": problem_class,
        "source": _source_fields(relative_path, optional),
        "present": present,
        "sha256": _sha256(absolute) if present else "",
    }
    return record


def curated_entries(optional: dict) -> list:
    records = []
    for suite, problem_class, name in CURATED:
        relative = f"data/{suite}/{name}.mps"
        records.append(_entry(relative, FAMILY_OF_SUITE.get(suite, suite), suite,
                              optional, problem_class))
    return records


def suite_entries(optional: dict) -> list:
    records = []
    for suite, spec in SUITES.items():
        directory = os.path.join(REPO, spec["dir"])
        glob_pattern = spec["glob"]
        local = sorted(os.path.basename(path) for path in
                       _glob(directory, glob_pattern)) if os.path.isdir(directory) else []
        seen = set()
        for filename in local:
            relative = f"{spec['dir']}/{filename}"
            seen.add(relative)
            records.append(_entry(relative, FAMILY_OF_SUITE.get(suite, suite),
                                  suite, optional))
        for relative in sorted(optional):
            entry = optional[relative]
            if (os.path.dirname(relative) == spec["dir"]
                    and _match(relative, glob_pattern) and relative not in seen):
                seen.add(relative)
                records.append(_entry(relative, FAMILY_OF_SUITE.get(suite, suite),
                                      suite, optional))
    return records


def _glob(directory: str, pattern: str) -> list:
    import fnmatch
    return [os.path.join(directory, name) for name in sorted(os.listdir(directory))
            if fnmatch.fnmatch(name, pattern)]


def _match(relative: str, pattern: str) -> bool:
    import fnmatch
    return fnmatch.fnmatch(os.path.basename(relative), pattern)


def assign_splits(records: list) -> dict:
    """Attach the frozen train/tune/holdout split to every record."""
    from scripts.ml.ml_splits import frozen_split
    ids = sorted({record["id"] for record in records})
    split = frozen_split(ids)
    lookup = {}
    for name, members in split["members"].items():
        for instance_id in members:
            lookup[instance_id] = name
    for record in records:
        record["split"] = lookup.get(record["id"], "train")
    return split


def build_manifest() -> dict:
    optional = _optional_index()
    curated = curated_entries(optional)
    suites = suite_entries(optional)
    split = assign_splits(curated + suites)
    unavailable = [record["id"] for record in curated + suites
                   if not record["present"]]
    return {
        "date": FROZEN_ON,
        "frozen_by": "scripts/support/frozen_instance_manifest.py",
        "offline": True,
        "downloads": "none; absent optional datasets are listed as unavailable",
        "family_tags": FAMILY_TAGS,
        "split": {
            "method": "scripts/ml/ml_splits.py frozen_split (seeded blake2b hash buckets)",
            "strategy": split["strategy"],
            "seed": split["seed"],
            "ratios": split["ratios"],
            "names": ["train", "tune", "holdout"],
            "alias_note": "ml_splits train/validation/test are reported here as "
                          "train/tune/holdout",
            "counts": split["counts"],
        },
        "timing": timing_as_dict(),
        "curated": curated,
        "suites": [
            {"suite": suite, "dir": spec["dir"], "glob": spec["glob"],
             "engine": spec["engine"],
             "instances": [record for record in suites if record["suite"] == suite]}
            for suite, spec in SUITES.items()
        ],
        "counts": {
            "curated": len(curated),
            "curated_present": sum(1 for record in curated if record["present"]),
            "suite_instances": len(suites),
            "suite_present": sum(1 for record in suites if record["present"]),
            "unavailable": len(unavailable),
        },
        "unavailable": sorted(unavailable),
    }


def write_manifest(path: str) -> str:
    payload = build_manifest()
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")
    return path
