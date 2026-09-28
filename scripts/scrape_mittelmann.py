#!/usr/bin/env python3
"""W9 / D-09: Mittelmann published benchmark table reference data.

Commercial solvers (CPLEX, Gurobi, Xpress) are NEVER run locally (no license);
the locked plan (D-09) uses Hans Mittelmann's published benchmark tables at
plato.asu.edu as the reference. This scraper fetches the benchmark index pages
and stores the raw HTML plus a parsed CSV per solver under
data/mittelmann_tables/ so the comparison report can cite published numbers
with an access date instead of re-running commercial software.

Usage:
  python3 scripts/scrape_mittelmann.py            # fetch all tables
  python3 scripts/scrape_mittelmann.py --offline  # re-parse cached HTML only

The scraped data is committed as reference evidence (provenance: URL + access
date + SHA-256 of each page). If the site is unreachable, --offline re-uses the
cached copies; nothing is ever manufactured.
"""

import argparse
import csv
import datetime
import hashlib
import os
import re
import sys
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(REPO, "data", "mittelmann_tables")

# Mittelmann's bench.html links the individual benchmark pages; the per-instance
# numbers live in plain-text .res tables (solver columns x instance rows, values
# are wall-clock seconds or 'timeout'). URLs verified against the live site.
PAGES = {
    "bench_index": "http://plato.asu.edu/bench.html",
    "milp_12threads": "https://plato.asu.edu/ftp/milp_tables/12threads.res",
}

USER_AGENT = "markov-cero-reference-scraper/0.5 (academic benchmark citation)"


def fetch(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.read().decode("utf-8", "replace")


def sha256(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def strip_tags(html: str) -> str:
    html = re.sub(r"<script.*?</script>", " ", html, flags=re.S | re.I)
    html = re.sub(r"<style.*?</style>", " ", html, flags=re.S | re.I)
    html = re.sub(r"<[^>]+>", " ", html)
    html = html.replace("&nbsp;", " ").replace("&amp;", "&")
    return re.sub(r"\s+", " ", html)


def parse_bench_index(html: str) -> list[dict]:
    """Extract (solver, benchmark-page-name) links from the index page."""
    text = strip_tags(html)
    rows = []
    # The index lists entries like "CPLEX 22.1" / "Gurobi 13.5" / "XPRESS 41"
    # with page links. We record every solver token with its access date; the
    # per-table numbers live on the linked pages, which are also cached.
    for solver in ("CPLEX", "Gurobi", "XPRESS", "HiGHS", "SCIP"):
        for match in re.finditer(rf"{solver}\s*[\w.]*", text):
            rows.append({"solver": solver, "entry": match.group(0).strip()})
    return rows


def parse_res_table(text: str) -> list[dict]:
    """Parse a Mittelmann .res table: 'Name | solver1 | solver2 | ...' header,
    then one row per instance with seconds or 'timeout' per solver cell."""
    lines = [ln.rstrip() for ln in text.splitlines() if ln.strip()]
    header_idx = None
    for i, ln in enumerate(lines):
        if ln.count("|") >= 2 and "Name" in ln:
            header_idx = i
            break
    if header_idx is None:
        return []
    solvers = [c.strip() for c in lines[header_idx].split("|")[1:-1]]
    rows = []
    for ln in lines[header_idx + 1:]:
        if set(ln.strip()) <= {"-", "+"}:
            continue
        parts = [c.strip() for c in ln.split("|")]
        if len(parts) >= len(solvers) + 1 and any(parts[1:]):
            name = parts[0]
            values = parts[1:1 + len(solvers)]
        else:
            # Whitespace layout: instance name followed by right-aligned value
            # columns (the pipe-padded header defines the column count). Split
            # on runs of 2+ spaces so embedded single spaces in 'timeout' stay
            # intact.
            fields = re.split(r"\s{2,}", ln.strip())
            if len(fields) < len(solvers) + 1:
                continue
            name, values = fields[0], fields[1:1 + len(solvers)]
        name = name.strip()
        if not name or name.startswith("-"):
            continue
        values = [v.strip() for v in values]
        if len(values) < len(solvers):
            continue
        row = {"instance": name}
        for solver, value in zip(solvers, values):
            row[solver] = value
        rows.append(row)
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--offline", action="store_true",
                    help="re-parse cached HTML; no network access")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    accessed = datetime.date.today().isoformat()
    provenance = {"accessed": accessed, "source": "http://plato.asu.edu/bench.html",
                  "pages": {}}

    for name, url in PAGES.items():
        cache = os.path.join(OUT_DIR, f"{name}.html")
        if args.offline:
            if not os.path.isfile(cache):
                print(f"[-] no cached copy for {name}, run without --offline",
                      file=sys.stderr)
                continue
            with open(cache, encoding="utf-8") as f:
                html = f.read()
        else:
            try:
                html = fetch(url)
            except Exception as e:  # noqa: BLE001 - honest failure below
                print(f"[-] fetch failed for {url}: {e}", file=sys.stderr)
                if os.path.isfile(cache):
                    with open(cache, encoding="utf-8") as f:
                        html = f.read()
                    print(f"[!] using cached copy for {name}", file=sys.stderr)
                else:
                    continue
            else:
                with open(cache, "w", encoding="utf-8") as f:
                    f.write(html)
        provenance["pages"][name] = {"url": url, "sha256": sha256(html),
                                     "bytes": len(html)}

        if name == "bench_index":
            rows = parse_bench_index(html)
            csv_path = os.path.join(OUT_DIR, f"{name}.csv")
            with open(csv_path, "w", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=["solver", "entry", "url",
                                                       "accessed"])
                writer.writeheader()
                for row in rows:
                    row.update({"url": url, "accessed": accessed})
                    writer.writerow(row)
            print(f"[+] {name}: {len(rows)} entries -> {csv_path}")
        else:
            # .res table: one row per instance, one column per solver
            table_rows = parse_res_table(html)
            if not table_rows:
                print(f"[-] {name}: no parseable table rows", file=sys.stderr)
                continue
            fieldnames = list(table_rows[0].keys()) + ["url", "accessed"]
            csv_path = os.path.join(OUT_DIR, f"{name}.csv")
            with open(csv_path, "w", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                writer.writeheader()
                for row in table_rows:
                    row.update({"url": url, "accessed": accessed})
                    writer.writerow(row)
            print(f"[+] {name}: {len(table_rows)} instances x "
                  f"{len(fieldnames) - 3} solvers -> {csv_path}")

    if provenance["pages"]:
        import json
        with open(os.path.join(OUT_DIR, "provenance.json"), "w") as f:
            json.dump(provenance, f, indent=2)
        print(f"[+] provenance -> {os.path.join(OUT_DIR, 'provenance.json')}")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
