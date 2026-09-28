#!/usr/bin/env python3
"""Materialize explicitly selected, hash-pinned optional benchmark files.

Default operation uses the preserved local cache or Git archive. --download
permits fetching the immutable source revision from origin when absent locally.
No build or test invokes this program implicitly.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def materialize(entry, download=False):
    relative = Path(entry['path'])
    if relative.is_absolute() or '..' in relative.parts or relative.parts[0] != 'data':
        raise ValueError('unsafe dataset path')
    target = ROOT / relative
    digest, size = entry['sha256'], entry['bytes']
    cache = ROOT / '.cache/dataset-store' / (digest + '.gz')
    if target.exists():
        with target.open("rb") as stream:
            raw = stream.read(size + 1)
    elif cache.exists():
        with gzip.open(cache, 'rb') as stream:
            raw = stream.read(size + 1)
    else:
        revision = entry['source_revision']
        if len(revision) != 40 or any(c not in '0123456789abcdef' for c in revision):
            raise ValueError('invalid archive revision')
        available = subprocess.run(['git', 'cat-file', '-e', revision + '^{commit}'],
            cwd=ROOT, capture_output=True).returncode == 0
        if not available and download:
            subprocess.run(['git', 'fetch', '--no-tags', '--depth=1', 'origin', revision],
                           cwd=ROOT, check=True, timeout=300)
        # The source revision reproduces transformations of upstream files too.
        with subprocess.Popen(['git', 'show', revision + ':' + relative.as_posix()],
                              cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL) as process:
            raw = process.stdout.read(size + 1)
            if len(raw) > size:
                process.kill()
            if process.wait(timeout=60) != 0:
                raise ValueError('dataset unavailable in local archive; use --download explicitly')
    if len(raw) != size or hashlib.sha256(raw).hexdigest() != digest:
        raise ValueError('dataset integrity mismatch: ' + str(relative))
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as out:
        temporary = Path(out.name)
        try:
            out.write(raw); out.flush(); temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--family', choices=['netlib', 'miplib', 'mittelmann', 'qp'])
    parser.add_argument('--name', help='Exact instance stem, e.g. gen-ip002')
    parser.add_argument('--download', action='store_true')
    parser.add_argument('--list', action='store_true')
    args = parser.parse_args()
    if not (args.family or args.name or args.list):
        parser.error('select --family or --name explicitly')
    entries = json.loads((ROOT / 'data/optional-datasets.json').read_text())
    selected = [e for e in entries if (not args.family or e['family'] == args.family)
                and (not args.name or Path(e['path']).stem == args.name)]
    if not selected: parser.error('no matching optional dataset')
    for entry in selected:
        if args.list: print(entry['path'], entry['bytes'], entry['sha256'])
        else: print(materialize(entry, args.download))


if __name__ == '__main__':
    main()
