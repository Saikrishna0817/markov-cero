from __future__ import annotations
from .download_mittelmann_suite_config import (
    DATA_DIR, LEGACY_SOURCES, LP_TARGET, MAX_COMPRESSED, MAX_STORED, MILP_RES, MILP_TARGET, MIPLIB_INSTANCE, REPO, REQUEST_TIMEOUT, USER_AGENT, argparse, json, os, pathlib, sys, time, urllib
)
from .download_mittelmann_suite_mps_stats import discover_lp_files
from .download_mittelmann_suite_mps_stats import discover_milp_names
from .download_mittelmann_suite_mps_stats import expand
from .download_mittelmann_suite_mps_stats import fetch
from .download_mittelmann_suite_mps_stats import format_note
from .download_mittelmann_suite_mps_stats import looks_like_mps
from .download_mittelmann_suite_mps_stats import mps_stats
from .download_mittelmann_suite_mps_stats import sha256_file

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
