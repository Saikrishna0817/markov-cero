#!/usr/bin/env python3
"""Write a successor frozen-instance manifest for a new campaign.

The 2026-09-28 freeze is immutable evidence; a campaign whose model files
changed afterwards must declare a *successor* manifest (benchmark campaign
contract section 3: "evidence/frozen-instances-20260928.json or its
successor"), never an edit of the original. This tool re-hashes the same
declared subset from disk and records exactly what moved.

    python3 scripts/freeze_successor_instances.py \
        --out evidence/frozen-instances-2026-10-01.json

What it guarantees:
  * the declared subset, suites, splits, families, classes and absence
    flags are copied field-for-field - a successor may change hashes, it
    may never change which instances are declared;
  * every declared-present file must exist and be readable, otherwise the
    tool refuses to write;
  * every hash difference is recorded by id with both digests;
  * a file that was absent at freeze time and now exists is reported and
    left absent (the subset stays frozen);
  * an existing output file is never overwritten.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_PREDECESSOR = REPO / 'evidence' / 'frozen-instances-20260928.json'


def sha256_file(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open('rb') as handle:
        for block in iter(lambda: handle.read(1 << 20), b''):
            hasher.update(block)
    return hasher.hexdigest()


def build(predecessor_path: Path, date: str) -> dict:
    manifest = json.loads(predecessor_path.read_text())
    changed, appeared, rehashed = [], [], 0
    for suite in manifest['suites']:
        for entry in suite['instances']:
            target = REPO / entry['id']
            if not entry.get('present'):
                if target.is_file():
                    appeared.append(entry['id'])
                continue
            if not target.is_file():
                raise SystemExit(
                    f'refusing to freeze: declared-present instance is '
                    f'missing from this checkout: {entry["id"]}')
            current = sha256_file(target)
            rehashed += 1
            frozen = entry.get('sha256', '')
            if not frozen:
                raise SystemExit(
                    f'refusing to freeze: {entry["id"]} carries no sha256 '
                    f'in the predecessor manifest')
            if current != frozen:
                changed.append({'id': entry['id'], 'frozen': frozen,
                                'current': current})
            entry['sha256'] = current
    successor = dict(manifest)
    successor['date'] = date
    successor['frozen_at'] = datetime.now(timezone.utc).isoformat(
        timespec='seconds')
    successor['successor_of'] = {
        'path': str(predecessor_path.relative_to(REPO)),
        'date': manifest['date'],
        'sha256': sha256_file(predecessor_path),
        'rule': 'successor manifests may re-hash files, never re-declare '
                'the subset (benchmark campaign contract section 3)',
    }
    successor['changed_vs_predecessor'] = changed
    successor['appeared_vs_predecessor'] = appeared
    successor['counts'] = dict(manifest.get('counts', {}))
    successor['counts']['rehashed_present'] = rehashed
    successor['counts']['changed_vs_predecessor'] = len(changed)
    successor['counts']['appeared_vs_predecessor'] = len(appeared)
    return successor


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--predecessor', default=str(DEFAULT_PREDECESSOR))
    parser.add_argument('--out', required=True)
    parser.add_argument('--date', required=True,
                        help='ISO date stamped into the successor manifest')
    args = parser.parse_args(argv)

    out = Path(args.out)
    if not out.is_absolute():
        out = REPO / out
    if out.exists():
        raise SystemExit(f'refusing to overwrite an existing freeze: {out}')
    predecessor = Path(args.predecessor)
    if not predecessor.is_absolute():
        predecessor = REPO / predecessor
    payload = build(predecessor, args.date)
    out.write_text(json.dumps(payload, indent=2) + '\n')
    print(f'wrote {out.relative_to(REPO)}: '
          f'{payload["counts"]["rehashed_present"]} present instances '
          f're-hashed, {len(payload["changed_vs_predecessor"])} changed, '
          f'{len(payload["appeared_vs_predecessor"])} appeared-but-absent')
    for item in payload['changed_vs_predecessor']:
        print(f'  changed {item["id"]} {item["frozen"][:12]} -> '
              f'{item["current"][:12]}')
    for item in payload['appeared_vs_predecessor']:
        print(f'  appeared (kept absent) {item}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
