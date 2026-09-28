#!/usr/bin/env python3
"""M6 / feature 34: MIPLIB 2017 *easy* benchmark set ingestion with provenance.

Discovers the official instance list from https://miplib.zib.de/ (the
benchmark-set table page, ``tag_benchmark.html``) and downloads each selected
instance from the site's per-instance archive, ``WebData/instances/{name}.mps.gz``.

Selection: instances whose site status is ``easy`` (solved within one hour in
the MIPLIB 2017 study), that carry a finite objective reference, that are not
already ingested in another suite under ``data/`` (Netlib / Mittelmann / QP /
MIPLIB), and that are small (variables + constraints <= 30000).  A strided
sample of ``--count`` instances is taken so the suite spans small through
medium sizes while every file stays well inside the 10 MB preference / 30 MB
hard cap.

Usage:
    python3 scripts/download_miplib_easy.py            # ingest + provenance
    python3 scripts/download_miplib_easy.py --list     # dry run selection
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
import pathlib
import random
import re
import sys
import time
import urllib.error
import urllib.request

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA_DIR = REPO / "data" / "miplib"
TABLE_URL = "https://miplib.zib.de/tag_benchmark.html"
INSTANCE_URL = "https://miplib.zib.de/WebData/instances/{name}.mps.gz"
USER_AGENT = "markov-cero-Dataset-Ingest"
PREFERRED_BYTES = 10 * 1024 * 1024   # keep instances <= 10 MB when possible
HARD_CAP_BYTES = 30 * 1024 * 1024    # never keep anything above 30 MB
MAX_DIM_SUM = 30_000                 # variables + constraints
REQUEST_TIMEOUT = 90
MAX_ATTEMPTS = 4


def fetch(url: str, attempts: int = MAX_ATTEMPTS) -> bytes:
    last: Exception | None = None
    for attempt in range(attempts):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
                return resp.read()
        except (urllib.error.HTTPError, urllib.error.URLError, TimeoutError, OSError) as exc:
            code = getattr(exc, "code", None)
            if isinstance(code, int) and code not in (408, 429) and code < 500:
                raise
            last = exc
            time.sleep((2 ** attempt) + random.random())
    raise RuntimeError(f"giving up on {url}: {last}")


def strip_tags(html: str) -> str:
    return re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", html)).strip()


def parse_table(html: str) -> list[dict]:
    """Parse the MIPLIB tag table: one row per instance with size + objective."""
    out: list[dict] = []
    for tr in re.findall(r"<tr[^>]*>(.*?)</tr>", html, re.S):
        cells = re.findall(r"<t[dh][^>]*>(.*?)</t[dh]>", tr, re.S)
        cells = [strip_tags(c) for c in cells]
        if len(cells) != 12 or cells[0] == "Instance":
            continue
        out.append(
            {
                "name": cells[0],
                "status": cells[1],
                "rows": int(float(cells[6])),
                "columns": int(float(cells[2])),
                "binaries": int(float(cells[3])),
                "integers": int(float(cells[4])),
                "nonzeros": int(float(cells[7])),
                "objective": cells[10],
            }
        )
    return out


def numeric(text: str) -> float | None:
    try:
        return float(text)
    except ValueError:
        return None


def existing_instances() -> set[str]:
    names: set[str] = set()
    for suite in ("netlib", "miplib", "mittelmann", "qp"):
        directory = REPO / "data" / suite
        if directory.is_dir():
            names.update(p.stem for p in directory.glob("*.mps"))
    return names


def select(count: int) -> list[dict]:
    rows = parse_table(fetch(TABLE_URL).decode("utf-8", "replace"))
    if not rows:
        raise RuntimeError("could not parse any rows from " + TABLE_URL)
    taken = existing_instances()
    cands = [
        r
        for r in rows
        if r["status"] == "easy"
        and numeric(r["objective"]) is not None
        and r["name"] not in taken
        and r["rows"] + r["columns"] <= MAX_DIM_SUM
    ]
    cands.sort(key=lambda r: (r["rows"] + r["columns"], r["name"]))
    if len(cands) <= count:
        return cands
    idx = sorted({round(i * (len(cands) - 1) / (count - 1)) for i in range(count)})
    return [cands[i] for i in idx]


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_provenance(target: pathlib.Path, meta: dict | None) -> None:
    name = target.stem
    # Legacy files the 2017 site does not host (e.g. the MIPLIB 2003 stein*
    # instances) have no upstream URL: record the on-disk path instead of
    # inventing one.
    record: dict = {
        "source": INSTANCE_URL.format(name=name) if meta else f"data/miplib/{target.name}",
        "member": target.name,
        "sha256": sha256_file(target),
        "name": name,
    }
    if meta:
        record["rows"] = meta["rows"]
        record["columns"] = meta["columns"]
        record["binaries"] = meta["binaries"]
        record["integers"] = meta["integers"]
        record["nonzeros"] = meta["nonzeros"]
        record["reference_objective"] = numeric(meta["objective"])
        record["suite"] = "MIPLIB 2017 (benchmark set, status easy)"
    else:
        record["rows"] = None
        record["columns"] = None
        record["reference_objective"] = None
        record["suite"] = "MIPLIB (pre-existing file, no site metadata found)"
    (DATA_DIR / f"{name}.provenance.json").write_text(
        json.dumps(record, indent=2) + "\n"
    )


def head_content_length(url: str) -> int | None:
    try:
        req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT}, method="HEAD")
        with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
            value = resp.headers.get("Content-Length")
            return int(value) if value else None
    except Exception:  # noqa: BLE001 - HEAD is only an optimisation
        return None


def ingest(selection: list[dict], table: dict[str, dict]) -> tuple[int, int, list[str]]:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    downloaded = reused = 0
    skipped: list[str] = []
    for i, meta in enumerate(selection, 1):
        name = meta["name"]
        target = DATA_DIR / f"{name}.mps"
        url = INSTANCE_URL.format(name=name)
        if not target.is_file():
            remote = head_content_length(url)
            if remote is not None and remote > HARD_CAP_BYTES:
                skipped.append(f"{name}: compressed {remote} B exceeds 30 MB cap")
                continue
            try:
                blob = fetch(url)
            except Exception as exc:  # noqa: BLE001
                skipped.append(f"{name}: download failed ({exc})")
                print(f"[-] ({i}/{len(selection)}) {name}: {exc}", file=sys.stderr)
                continue
            if len(blob) > HARD_CAP_BYTES:
                skipped.append(f"{name}: compressed {len(blob)} B exceeds 30 MB cap")
                continue
            try:
                raw = gzip.decompress(blob)
            except OSError as exc:
                skipped.append(f"{name}: gzip decode failed ({exc})")
                continue
            if len(raw) > HARD_CAP_BYTES:
                skipped.append(f"{name}: decompressed {len(raw)} B exceeds 30 MB cap")
                continue
            if len(raw) > PREFERRED_BYTES:
                print(f"[!] ({i}/{len(selection)}) {name}: {len(raw)} B > 10 MB preference")
            tmp = target.with_suffix(".mps.part")
            tmp.write_bytes(raw)
            os.replace(tmp, target)
            downloaded += 1
            print(f"[+] ({i}/{len(selection)}) {name}: {len(raw)} bytes")
        else:
            reused += 1
            print(f"[=] ({i}/{len(selection)}) {name}: already on disk")
        write_provenance(target, table.get(name, meta))
        time.sleep(0.15)
    return downloaded, reused, skipped


def site_metadata(names: list[str]) -> dict[str, dict]:
    """Prefer the full (unfiltered) easy-tag table for provenance metadata."""
    try:
        html = fetch("https://miplib.zib.de/tag_easy.html").decode("utf-8", "replace")
    except Exception:  # noqa: BLE001 - fall back to the selection metadata
        return {}
    table = {r["name"]: r for r in parse_table(html)}
    return {n: table[n] for n in names if n in table}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--count", type=int, default=42, help="number of instances to ingest")
    ap.add_argument("--list", action="store_true", help="only print the selection")
    args = ap.parse_args()

    selection = select(args.count)
    if args.list:
        for m in selection:
            print(m["name"], m["rows"], m["columns"], m["objective"])
        print(f"# {len(selection)} selected", file=sys.stderr)
        return 0

    table = site_metadata([m["name"] for m in selection])
    # The three pre-existing files also need provenance; look them up too.
    for legacy in ("flugpl", "stein9", "stein15"):
        if legacy not in table:
            table.update(site_metadata([legacy]))
    print(f"[*] {len(selection)} MIPLIB easy instances targeted")
    downloaded, reused, skipped = ingest(selection, table)

    for legacy in ("flugpl", "stein9", "stein15"):
        target = DATA_DIR / f"{legacy}.mps"
        if target.is_file() and not (DATA_DIR / f"{legacy}.provenance.json").is_file():
            write_provenance(target, table.get(legacy))

    print(f"[+] downloaded={downloaded} reused={reused} skipped={len(skipped)}")
    for line in skipped:
        print(f"[!] {line}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
