#!/usr/bin/env python3
"""M6 / feature 33: full Netlib LP suite ingestion with SHA-256 provenance.

Downloads every ``{name}.mps.gz`` member of coin-or-tools/Data-Netlib from
raw.githubusercontent.com (not rate limited), gunzips it into
``data/netlib/{name}.mps`` and writes ``{name}.provenance.json`` for *every*
member (pre-existing files are hashed in place, never overwritten).

Usage:
    python3 scripts/download_netlib_suite.py           # ingest + provenance
    python3 scripts/download_netlib_suite.py --list    # dry run

Network hygiene: proper User-Agent, per-request timeout, small sleeps between
requests, exponential backoff with jitter on transient failures.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
import pathlib
import random
import sys
import time
import urllib.error
import urllib.request

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA_DIR = REPO / "data" / "netlib"
RAW_BASE = "https://raw.githubusercontent.com/coin-or-tools/Data-Netlib/master"
API_URL = "https://api.github.com/repos/coin-or-tools/Data-Netlib/contents/?per_page=1000"
USER_AGENT = "markov-cero-Dataset-Ingest"
MAX_DECOMPRESSED = 60 * 1024 * 1024  # 60 MB guard from the ingestion spec
REQUEST_TIMEOUT = 60
MAX_ATTEMPTS = 4

sys.path.insert(0, str(REPO / "scripts"))
try:
    from run_netlib import NETLIB_BENCHMARKS  # type: ignore
except Exception:  # pragma: no cover - reference table always ships with repo
    NETLIB_BENCHMARKS = {}


def fetch(url: str, attempts: int = MAX_ATTEMPTS) -> bytes:
    last: Exception | None = None
    for attempt in range(attempts):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
                return resp.read()
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError, OSError) as exc:
            code = getattr(exc, "code", None)
            if isinstance(code, int) and code not in (408, 429, 500, 502, 503, 504) and code < 500:
                raise
            last = exc
            time.sleep((2 ** attempt) + random.random())
    raise RuntimeError(f"giving up on {url}: {last}")


def list_members() -> list[str]:
    payload = json.loads(fetch(API_URL))
    if not isinstance(payload, list):
        raise RuntimeError(f"unexpected GitHub API payload: {payload!r}")
    return sorted(
        entry["name"][: -len(".mps.gz")]
        for entry in payload
        if entry.get("type") == "file" and entry["name"].endswith(".mps.gz")
    )


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def provenance_for(name: str, mps_path: pathlib.Path) -> dict:
    meta = NETLIB_BENCHMARKS.get(name) or {}
    return {
        "source": f"{RAW_BASE}/{name}.mps.gz",
        "member": f"{name}.mps",
        "sha256": sha256_file(mps_path),
        "name": name,
        "reference_objective": meta.get("optimal"),
    }


def ingest(names: list[str]) -> tuple[int, int, list[str]]:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    downloaded = reused = 0
    skipped: list[str] = []
    for idx, name in enumerate(names, 1):
        target = DATA_DIR / f"{name}.mps"
        url = f"{RAW_BASE}/{name}.mps.gz"
        if not target.is_file():
            try:
                blob = fetch(url)
            except Exception as exc:  # noqa: BLE001 - report and continue
                skipped.append(f"{name}: download failed ({exc})")
                print(f"[-] ({idx}/{len(names)}) {name}: {exc}", file=sys.stderr)
                continue
            try:
                raw = gzip.decompress(blob)
            except OSError as exc:
                skipped.append(f"{name}: gzip decode failed ({exc})")
                continue
            if len(raw) > MAX_DECOMPRESSED:
                skipped.append(
                    f"{name}: decompressed size {len(raw)} B exceeds 60 MB cap"
                )
                print(f"[!] ({idx}/{len(names)}) {name}: skipped, {len(raw)} B > 60 MB")
                continue
            tmp = target.with_suffix(".mps.part")
            tmp.write_bytes(raw)
            os.replace(tmp, target)
            downloaded += 1
            print(f"[+] ({idx}/{len(names)}) {name}: {len(raw)} bytes")
        else:
            reused += 1
            print(f"[=] ({idx}/{len(names)}) {name}: already on disk")
        prov_path = DATA_DIR / f"{name}.provenance.json"
        prov_path.write_text(json.dumps(provenance_for(name, target), indent=2) + "\n")
        time.sleep(0.15)
    return downloaded, reused, skipped


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", action="store_true", help="only list remote members")
    ap.add_argument("--limit", type=int, default=0, help="ingest only first N")
    args = ap.parse_args()

    names = list_members()
    if args.list:
        for n in names:
            print(n)
        print(f"# {len(names)} members", file=sys.stderr)
        return 0
    if args.limit:
        names = names[: args.limit]
    print(f"[*] {len(names)} Netlib members targeted")
    downloaded, reused, skipped = ingest(names)
    print(
        f"[+] downloaded={downloaded} reused={reused} skipped={len(skipped)}"
    )
    for line in skipped:
        print(f"[!] {line}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
