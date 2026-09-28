#!/usr/bin/env python3
"""M6 / feature 35: Mittelmann benchmark suite ingestion with SHA-256 provenance.

Hans Mittelmann's benchmark index (https://plato.asu.edu/bench.html) links the
per-family pages; the LP pages (``ftp/lpopt.html``, ``ftp/lpfeas.html``) state
that the MPS data files live in ``ftp/lptestset/`` (see its ``00README``), and
the MILP page (``ftp/milp.html``) links the result table
``ftp/milp_tables/12threads.res`` whose instance names are MIPLIB 2017
instances.

This script:
  1. lists the ``lptestset/`` directories (Apache indexes) to discover file URLs,
  2. downloads the selected instances (bz2/gz archives), expanding the
     MPC-compressed ones with the netlib EMPS utility where required,
  3. stores them as ``data/mittelmann/{name}.mps`` and writes
     ``{name}.provenance.json``,
  4. writes provenance for the six pre-existing members as well.

Size policy: compressed <= 6 MB, stored (expanded MPS) <= 15 MB.

Usage:
    python3 scripts/download_mittelmann_suite.py            # ingest + provenance
    python3 scripts/download_mittelmann_suite.py --list     # dry run discovery
"""
from __future__ import annotations

import argparse
import bz2
import gzip
import hashlib
import json
import os
import pathlib
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from urllib.parse import urljoin

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA_DIR = REPO / "data" / "mittelmann"
MILP_PAGE = "https://plato.asu.edu/ftp/milp.html"
MILP_RES = "https://plato.asu.edu/ftp/milp_tables/12threads.res"
LPTESTSET = "https://plato.asu.edu/ftp/lptestset/"
LPTESTSET_DIRS = ("", "misc/", "pds/", "nug/", "fome/", "network/", "rail/", "fctp/")
MIPLIB_INSTANCE = "https://miplib.zib.de/WebData/instances/{name}.mps.gz"
USER_AGENT = "markov-cero-Dataset-Ingest"
MAX_COMPRESSED = 6 * 1024 * 1024
MAX_STORED = 15 * 1024 * 1024
REQUEST_TIMEOUT = 90
MAX_ATTEMPTS = 4
LP_TARGET = 10
MILP_TARGET = 4

# Provenance of the six members that predate this ingestion (verified in
# 2026-09 by re-downloading the upstream copy and comparing contents).
LEGACY_SOURCES = {
    "bienst1": (
        MIPLIB_INSTANCE.format(name="bienst1"),
        "identical to the MIPLIB copy apart from trailing whitespace on the NAME line",
    ),
    "bienst2": (
        MIPLIB_INSTANCE.format(name="bienst2"),
        "identical to the MIPLIB copy apart from trailing whitespace on the NAME line",
    ),
    "markshare_5_0": (
        MIPLIB_INSTANCE.format(name="markshare_5_0"),
        "identical to the MIPLIB copy apart from the NAME line (MPSDATA vs markshare_5_0)",
    ),
    "mkc1": (
        MIPLIB_INSTANCE.format(name="mkc1"),
        "identical to the MIPLIB copy apart from trailing whitespace on the NAME line",
    ),
    "neos5": (
        MIPLIB_INSTANCE.format(name="neos5"),
        "byte-identical to the MIPLIB copy",
    ),
    "ran14x18_1": (
        "data/mittelmann/ran14x18_1.mps",
        "no upstream URL found (legacy MIPLIB-era instance); on-disk path recorded",
    ),
}


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


def text(url: str) -> str:
    return fetch(url).decode("utf-8", "replace")


def looks_like_mps(raw: bytes) -> bool:
    """NAME first, plus a ROWS section and an ENDATA terminator.

    The check matters because ``lptestset/`` also serves MPC-compressed
    payloads: those start with a NAME line too, but carry no MPS sections
    (see lptestset/00README).
    """
    saw_name = saw_rows = False
    saw_endata = False
    for line in raw.split(b"\n"):
        stripped = line.strip()
        if not stripped or stripped.startswith(b"*"):
            continue
        if not saw_name:
            if not stripped.startswith(b"NAME"):
                return False
            saw_name = True
            continue
        if stripped == b"ROWS":
            saw_rows = True
        elif stripped.startswith(b"ENDATA"):
            saw_endata = True
        if saw_rows and saw_endata:
            return True
    return saw_name and saw_rows and saw_endata


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def mps_stats(path: pathlib.Path) -> dict:
    """rows / columns / constraint nonzeros / integrality of an MPS file."""
    section = None
    row_types: dict[str, str] = {}
    objective = None
    cols: set[str] = set()
    integers: set[str] = set()
    nnz = 0
    obj_nnz = 0
    current_int = False
    for raw in path.read_text(errors="replace").splitlines():
        if not raw.strip() or raw.lstrip().startswith("*"):
            continue
        tokens = raw.strip().split()
        key = tokens[0]
        if key == "NAME":
            continue
        if key in ("ROWS", "COLUMNS", "RHS", "RANGES", "BOUNDS", "ENDATA",
                   "OBJSENSE", "OBJNAME"):
            section = key
            continue
        if section == "ROWS" and len(tokens) >= 2:
            row_types[tokens[1]] = tokens[0]
            if tokens[0] == "N":
                objective = tokens[1]
        elif section == "COLUMNS":
            if len(tokens) >= 3 and tokens[1] == "'MARKER'":
                current_int = tokens[-1].strip("'") == "INTORG"
                continue
            if len(tokens) >= 3:
                cols.add(tokens[0])
                if current_int:
                    integers.add(tokens[0])
                if objective is not None and tokens[1] == objective:
                    obj_nnz += 1
                else:
                    nnz += 1
        elif section == "BOUNDS" and len(tokens) >= 3 and tokens[0] in (
            "BV", "LI", "MI", "UI", "SI"
        ):
            #  UP/LO/FX/PL/FR lines carry a value, BV lines do not: the column
            #  name is always the third token.
            integers.add(tokens[2])
            cols.add(tokens[2])
    rows = sum(1 for kind in row_types.values() if kind != "N")
    return {
        "rows": rows,
        "columns": len(cols),
        "nonzeros": nnz,
        "objective_nonzeros": obj_nnz,
        "integer_columns": len(integers),
        "problem_type": "MILP" if integers else "LP",
    }


def decompress(blob: bytes, url: str) -> bytes:
    if url.endswith(".bz2"):
        return bz2.decompress(blob)
    if url.endswith(".gz"):
        return gzip.decompress(blob)
    if url.endswith(".xz"):
        import lzma
        return lzma.decompress(blob)
    return blob


# lptestset/00README: "The files w/o mps subscript are compressed with the MPC
# utility and need to be uncompressed with EMPS to be converted to MPS format.
# Both utilities are available at http://www.netlib.org/lp/data/".
EMPS_SOURCE = "http://www.netlib.org/lp/data/emps.c"


def is_mpc(url: str) -> bool:
    """True for lptestset archives holding MPC (not plain MPS) payloads."""
    return "lptestset" in url and not re.search(r"\.mps\.(bz2|gz|xz)$", url, re.I)


def ensure_emps() -> pathlib.Path | None:
    """Build the netlib EMPS expander once, outside the repository."""
    override = os.environ.get("EMPS")
    if override:
        return pathlib.Path(override)
    cached = pathlib.Path(tempfile.gettempdir()) / "markov_cero_emps"
    if cached.is_file() and os.access(cached, os.X_OK):
        return cached
    compiler = shutil.which("cc") or shutil.which("gcc")
    if compiler is None:
        return None
    try:
        source = fetch(EMPS_SOURCE)
    except Exception as exc:  # noqa: BLE001
        print(f"[!] cannot fetch {EMPS_SOURCE}: {exc}", file=sys.stderr)
        return None
    workdir = pathlib.Path(tempfile.mkdtemp(prefix="markov_emps_"))
    src = workdir / "emps.c"
    src.write_bytes(source)
    build = subprocess.run(
        [compiler, "-O2", "-o", str(cached), str(src)],
        capture_output=True,
        text=True,
    )
    if build.returncode != 0:
        print(f"[!] emps build failed: {build.stderr[-400:]}", file=sys.stderr)
        return None
    return cached


def format_note(url: str) -> str:
    if is_mpc(url):
        return (
            "bzip2/gzip archive holding an MPC-compressed MPS file, expanded with "
            + EMPS_SOURCE
        )
    return f"archive of a plain MPS file"


def expand(blob: bytes, url: str) -> tuple[bytes, str]:
    """Archive -> plain MPS bytes; returns (payload, provenance note)."""
    payload = decompress(blob, url)
    if not is_mpc(url):
        return payload, format_note(url)
    emps = ensure_emps()
    if emps is None:
        raise RuntimeError("MPC archive needs the EMPS expander (see lptestset/00README)")
    run = subprocess.run(
        [str(emps)], input=payload, capture_output=True, timeout=600
    )
    if run.returncode != 0 or not run.stdout:
        raise RuntimeError(f"EMPS failed on {url}: {run.stderr[:300]!r}")
    return run.stdout, format_note(url)


def list_directory(base: str) -> list[dict]:
    """Parse an Apache ``Indexes`` listing into [{url, size}] entries."""
    html = text(base)
    out = []
    for tr in re.findall(r"<tr>(.*?)</tr>", html, re.S | re.I):
        cells = re.findall(r"<td[^>]*>(.*?)</td>", tr, re.S | re.I)
        if len(cells) < 4:
            continue
        href_match = re.search(r'href="([^"]+)"', cells[1])
        if not href_match:
            continue
        href = href_match.group(1)
        if href.startswith("?") or href.endswith("/") or not re.search(
            r"\.(mps\.)?(bz2|gz|xz|mps)$", href, re.I
        ):
            continue
        size = re.sub(r"<[^>]+>", "", cells[3]).strip()
        out.append({"url": urljoin(base, href), "size": size})
    return out


SKIP_LP_TOKENS = {
    "probs", "scaled", "unscaled", "solved", "name", "instance", "testset",
    "notes", "note", "the", "this", "logfiles", "last",
}


def strip_archive(filename: str) -> str:
    for suffix in (".mps.bz2", ".mps.gz", ".mps.xz", ".bz2", ".gz", ".xz", ".mps"):
        if filename.endswith(suffix):
            return filename[: -len(suffix)]
    return filename


def discover_lp_files() -> list[dict]:
    """Every instance file under ftp/lptestset/, in directory order.

    Files without an ``.mps`` part of the name are MPC-compressed (see
    ``lptestset/00README``); each entry records the index page that lists it
    so the provenance can point at the listing rather than the raw file.
    """
    files: list[dict] = []
    for sub in LPTESTSET_DIRS:
        base = urljoin(LPTESTSET, sub)
        for entry in list_directory(base):
            entry["index"] = base
            entry["name"] = strip_archive(pathlib.PurePosixPath(entry["url"]).name)
            files.append(entry)
        time.sleep(0.3)
    return files


def discover_milp_names() -> list[str]:
    body = text(MILP_PAGE)
    if MILP_RES not in body:
        raise RuntimeError("MILP result table link not found on " + MILP_PAGE)
    res = text(MILP_RES)
    names = []
    for line in res.splitlines():
        stripped = line.strip()
        if stripped.startswith("p_"):
            name = stripped.split()[0][2:]
            if name and name not in names:
                names.append(name)
    return names


def existing_elsewhere() -> set[str]:
    taken: set[str] = set()
    for suite in ("netlib", "miplib", "qp"):
        directory = REPO / "data" / suite
        if directory.is_dir():
            taken.update(p.stem for p in directory.glob("*.mps"))
    return taken


def write_provenance(target: pathlib.Path, source: str, page: str | None = None,
                     note: str | None = None) -> dict:
    stats = mps_stats(target)
    record = {
        "source": source,
        "member": target.name,
        "sha256": sha256_file(target),
        "name": target.stem,
        "rows": stats["rows"],
        "columns": stats["columns"],
        "nonzeros": stats["nonzeros"],
        "problem_type": stats["problem_type"],
    }
    if page:
        record["listed_on"] = page
    if note:
        record["note"] = note
    (target.parent / f"{target.stem}.provenance.json").write_text(
        json.dumps(record, indent=2) + "\n"
    )
    return record


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", action="store_true", help="only print the discovery results")
    ap.add_argument("--lp-target", type=int, default=LP_TARGET)
    ap.add_argument("--milp-target", type=int, default=MILP_TARGET)
    args = ap.parse_args()

    taken = existing_elsewhere()
    lp_files = discover_lp_files()
    milp_names = discover_milp_names()
    if args.list:
        print(
            f"# {len(lp_files)} lptestset files, {len(milp_names)} MILP table instances"
        )
        for f in lp_files:
            print("FILE", f["url"], f["size"], f["index"])
        return 0

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    # a previous run may have stored an MPC payload as if it were MPS
    for stale in sorted(DATA_DIR.glob("*.mps")):
        if not looks_like_mps(stale.read_bytes()):
            print(f"[!] dropping unusable copy {stale.name}")
            stale.unlink()
            provenance = stale.parent / f"{stale.stem}.provenance.json"
            if provenance.is_file():
                provenance.unlink()

    skipped: list[str] = []
    downloaded = reused = 0
    lp_done = milp_done = 0

    def ingest(name: str, url: str, page: str | None) -> bool:
        nonlocal downloaded, reused
        target = DATA_DIR / f"{name}.mps"
        if target.is_file() and looks_like_mps(target.read_bytes()):
            reused += 1
            write_provenance(target, url, page, format_note(url))
            print(f"[=] {name}: already on disk")
            return True
        if target.is_file():
            print(f"[=] {name}: unusable copy on disk, re-downloading")
        try:
            blob = fetch(url)
        except Exception as exc:  # noqa: BLE001
            skipped.append(f"{name}: download failed ({exc})")
            print(f"[-] {name}: {exc}", file=sys.stderr)
            return False
        if len(blob) > MAX_COMPRESSED:
            skipped.append(f"{name}: compressed {len(blob)} B exceeds 6 MB")
            return False
        try:
            raw, note = expand(blob, url)
        except Exception as exc:  # noqa: BLE001
            skipped.append(f"{name}: expansion failed ({exc})")
            return False
        if len(raw) > MAX_STORED:
            skipped.append(f"{name}: expanded {len(raw)} B exceeds 15 MB")
            print(f"[!] {name}: skipped, {len(raw)} B > 15 MB")
            return False
        if not looks_like_mps(raw):
            skipped.append(f"{name}: expanded payload is not an MPS file")
            print(f"[!] {name}: skipped, payload is not MPS")
            return False
        tmp = target.with_suffix(".mps.part")
        tmp.write_bytes(raw)
        os.replace(tmp, target)
        downloaded += 1
        write_provenance(target, url, page, note)
        print(f"[+] {name}: {len(raw)} bytes")
        time.sleep(0.15)
        return True

    def indexed_size(entry: dict) -> float:
        label = (entry.get("size") or "").upper().replace("B", "")
        try:
            value = float(label.rstrip("KMG"))
        except ValueError:
            return 0.0
        factor = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30}.get(label[-1:], 1)
        return value * factor

    # --- LP instances, in ftp/lptestset/ directory order --------------------
    for entry in lp_files:
        if lp_done >= args.lp_target:
            break
        name = entry["name"]
        if name in taken:
            continue
        if indexed_size(entry) > MAX_COMPRESSED:
            skipped.append(f"{name}: indexed size {entry['size']} exceeds 6 MB")
            continue
        if ingest(name, entry["url"], entry["index"]):
            lp_done += 1

    # --- MILP instances from the 12-thread result table --------------------
    for name in milp_names:
        if milp_done >= args.milp_target:
            break
        if name in taken:
            continue
        url = MIPLIB_INSTANCE.format(name=name)
        target = DATA_DIR / f"{name}.mps"
        if target.is_file():
            if ingest(name, url, MILP_RES):
                milp_done += 1
            continue
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT}, method="HEAD")
            with urllib.request.urlopen(req, timeout=REQUEST_TIMEOUT) as resp:
                length = int(resp.headers.get("Content-Length") or 0)
        except Exception:  # noqa: BLE001
            continue
        if length > MAX_COMPRESSED:
            skipped.append(f"{name}: compressed {length} B exceeds 6 MB")
            continue
        if ingest(name, url, MILP_RES):
            milp_done += 1

    # --- provenance for the members that already existed -------------------
    for name, (source, note) in LEGACY_SOURCES.items():
        target = DATA_DIR / f"{name}.mps"
        if target.is_file():
            write_provenance(target, source, None, note)

    print(
        f"[+] downloaded={downloaded} reused={reused} "
        f"lp={lp_done} milp={milp_done} skipped={len(skipped)}"
    )
    for line in skipped:
        print(f"[!] {line}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
