#!/usr/bin/env python3
"""Generate the canonical large LP cases used by run_crossover_large.py.

This is the plan-facing generation entry point.  The crossover runner owns
the instance topology and size specification so generation and benchmarking
cannot silently drift apart.  Generated files are deterministic and stored
under data/scale_study/ (gitignored); only benchmark evidence is committed.

Usage:
    python3 scripts/generate_large_scale_study.py [--target data/scale_study]
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import run_crossover_large as study


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", default="data/scale_study")
    args = parser.parse_args()

    target = Path(args.target)
    target.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, rows, bandwidth, seed in study.SPECS:
        # Regenerate even if a file already exists: older versions of the
        # plan-facing script used a different topology under the same names.
        for ext in ("mps", "lp"):
            stale = target / f"{name}.{ext}"
            if stale.exists():
                stale.unlink()
        info = study.generate_instance(name, rows, bandwidth, seed, str(target))
        path = Path(info["path"])
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        manifest[name] = {
            **info,
            "sha256": digest,
            "generator": "scripts/run_crossover_large.py",
            "known_feasible_point": "x_j = 1 for every variable",
            "row_rank": "full row rank (upper triangular leading block)",
        }
        print(f"{name}: rows={rows}, cols={info['cols']}, nnz={info['nnz']}, "
              f"sha256={digest}")

    manifest_path = target / "large_scale_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print(f"manifest written: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
