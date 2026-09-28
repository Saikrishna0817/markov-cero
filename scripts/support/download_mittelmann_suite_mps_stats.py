from __future__ import annotations
from .download_mittelmann_suite_config import (
    EMPS_SOURCE, LPTESTSET, LPTESTSET_DIRS, MAX_ATTEMPTS, MILP_PAGE, MILP_RES, REQUEST_TIMEOUT, USER_AGENT, bz2, gzip, hashlib, os, pathlib, random, re, shutil, subprocess, sys, tempfile, time, urljoin, urllib
)

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
