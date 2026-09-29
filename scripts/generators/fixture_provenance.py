"""Deterministic provenance for generated data/cases fixtures."""

import hashlib
import json
from pathlib import Path


REGENERATION_DATE = "2026-09-29"


def write_provenance(output, generator, arguments, change, existing=None):
    output = Path(output)
    record = dict(existing or {})
    record.update({
        "fixture": output.name,
        "generator": generator,
        "generator_args": arguments,
        "regeneration_date": REGENERATION_DATE,
        "change": change,
        "model_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
    })
    output.with_suffix(".provenance.json").write_text(
        json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
