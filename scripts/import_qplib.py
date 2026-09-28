#!/usr/bin/env python3
"""QPLIB (.qplib) -> markov-cero MPS importer (R19: dataset ingestion + provenance).

QPLIB distributes each instance as a single ``.qplib`` text file (optionally
zipped). The format is positional and self-describing:

    line 1            name + free-text provenance
    line 2            problem class token (e.g. QBL/QCL/...)
    line 3            Minimize | Maximize
    line 4            n (variables)
    line 5            m (general linear constraints)
    <hnnz>            nonzeros in Q0 (row, col, value)
    <gnnz>            non-default linear objective entries (index, value)
    <annz>            nonzeros of A (row, col, value)
    <c_l>, <c_u>      row bound overrides (index, value)
    <x_l>, <x_u>      variable bound overrides (index, value)
    <x>, <y>, <z>     known points (index, value) + default values
    names             optional non-default names

QPLIB's own definition (qplib.zib.de/doc.html) is

    sense  1/2 x^T Q0 x + g^T x + c     s.t.  c_l <= A x <= c_u,
                                                   x_l <= x <= x_u

where Q0 is a *lower-left triangle* matrix (Q0[row, col] = 0 for row < col):
an off-diagonal term therefore contributes 1/2 * value * x_row * x_col.  The
MPS ``QUADOBJ`` convention is different — it names the upper triangle of a
*symmetric* Hessian, which the reader mirrors to (col, row) before the
1/2 x^T Q x evaluation.  ``parse_annotated`` closes that gap by halving the
off-diagonal terms and folding them onto i <= j, after which the importer is a
structural transcode: linear objective -> the ``N`` row, H -> ``QUADOBJ``,
row bounds -> ``RANGES``, variable bounds -> ``BOUNDS``.

Provenance: every emitted instance is accompanied by a ``.provenance.json``
recording the source file, its sha256, the QPLIB class, dimensions, and the
objective value of QPLIB's official solution point when one is published
(``sol/QPLIB_xxxx.sol``), together with the path of that solution file.  The
reference objective is only published after re-evaluating the solution point
against the parsed model and finding the result equal to the file's ``objvar``
record — that check is what pins the quadratic convention.

Two dialects of the same layout are read: files that annotate each record with
its ``# label`` comment (the ones served by qplib.zib.de) and comment-less
generated instances.  The labels mark the records QPLIB omits when they carry
no information — constraint rows, row bounds and starting points of an
unconstrained instance, for example — which a purely positional read would
silently shift.  Generated instances already store the upper triangle of a
symmetric Hessian and are passed through unchanged.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys
import zipfile

INF = float("inf")


class Reader:
    """Line-granular reader for the positional QPLIB body.

    The format mixes numeric records with prose trailers on the same line
    (``0.0   default value for entries in initial x``), so records must be read
    *line by line*, taking only the leading tokens that are numeric.
    """

    def __init__(self, lines: list[str]):
        self.lines = lines
        self.pos = 0

    def fields(self) -> list[str]:
        while self.pos < len(self.lines):
            tokens = self.lines[self.pos].split()
            self.pos += 1
            if tokens:
                return tokens
        raise ValueError("unexpected end of QPLIB file")

    def integer(self) -> int:
        return int(float(self.fields()[0]))

    def number(self) -> float:
        return float(self.fields()[0])

    def pairs(self, count: int) -> dict[int, float]:
        out: dict[int, float] = {}
        for _ in range(count):
            tokens = self.fields()
            out[int(float(tokens[0]))] = float(tokens[1])
        return out

    def triples(self, count: int) -> list[tuple[int, int, float]]:
        out = []
        for _ in range(count):
            tokens = self.fields()
            out.append((int(float(tokens[0])), int(float(tokens[1])), float(tokens[2])))
        return out


def split_records(path: pathlib.Path) -> list[tuple[str, str]]:
    """Split a QPLIB file into ``(data, comment)`` records.

    Two dialects exist: QPLIB's own distribution annotates every positional
    record with its ``# ...`` label ("number of constraints", "default
    variable lower bound value", ...), while generated instances omit the
    comments entirely.  The labels are what makes optional records (constraint
    data, starting points, variable bounds) distinguishable from mandatory
    ones, so they are kept.
    """
    records: list[tuple[str, str]] = []
    for line in path.read_text(errors="replace").splitlines():
        data, _, comment = line.partition("#")
        data = data.strip()
        if data:
            records.append((data, comment.strip().lower()))
    return records


class LabelReader:
    """Cursor over an *annotated* QPLIB body (every record carries its label).

    ``section(label)`` reads a ``default`` + ``count`` section only when the
    next record's own comment identifies it, returning ``None`` for records
    the writer omitted (constraint data for an unconstrained instance,
    starting points, ...).  Skipping an unrecognised record is never done
    silently: the caller raises, so a dialect change surfaces as a per-instance
    import failure instead of a shifted parse.
    """

    def __init__(self, records: list[tuple[str, str]]):
        self.records = records
        self.pos = 0

    def comment(self) -> str:
        if self.pos >= len(self.records):
            return ""
        return self.records[self.pos][1]

    def record(self) -> str:
        if self.pos >= len(self.records):
            raise ValueError("unexpected end of QPLIB file")
        text = self.records[self.pos][0]
        self.pos += 1
        return text

    def fields(self) -> list[str]:
        tokens = self.record().split()
        if not tokens:
            raise ValueError("unexpected end of QPLIB file")
        return tokens

    def integer(self) -> int:
        return int(float(self.fields()[0]))

    def number(self) -> float:
        return float(self.fields()[0])

    def pairs(self, count: int) -> dict[int, float]:
        out: dict[int, float] = {}
        for _ in range(count):
            tokens = self.fields()
            out[int(float(tokens[0]))] = float(tokens[1])
        return out

    def triples(self, count: int) -> list[tuple[int, int, float]]:
        out = []
        for _ in range(count):
            tokens = self.fields()
            out.append((int(float(tokens[0])), int(float(tokens[1])), float(tokens[2])))
        return out

    def section(self, label: str) -> tuple[float, dict[int, float]] | None:
        if label not in self.comment():
            return None
        default = self.number()
        return default, self.pairs(self.integer())


def parse_annotated(body: list[tuple[str, str]], path: pathlib.Path) -> dict:
    """Label-driven parse of QPLIB's own distribution files."""
    r = LabelReader(body)
    n = r.integer()                                   # number of variables
    m = r.integer() if "number of constraints" in r.comment() else 0

    h_entries = r.triples(r.integer())
    # QPLIB defines the objective as 1/2 x^T Q0 x + g^T x + c with Q0 a
    # *lower-left triangle* matrix (Q0[row, col] = 0 for row < col), so a stored
    # off-diagonal term contributes exactly 1/2 * value * x_row * x_col.  Every
    # consumer below speaks the symmetric-Hessian dialect instead (one
    # triangle, 1/2 x^T Q x), which needs off-diagonal entries halved and
    # folded onto i <= j; diagonal terms are already in that form.  Halving by
    # 0.5 is exact in binary floating point.
    h_entries = [(i, j, v if i == j else 0.5 * v)
                 for i, j, v in ((min(i, j), max(i, j), v) for i, j, v in h_entries)]
    g_default = r.number()                            # default value for entries in g
    g = r.pairs(r.integer())
    objective_offset = r.number()                     # objective constant
    # Constraint matrix: omitted together with every constraint record when
    # the instance has no linear constraints (m == 0).
    a_entries = r.triples(r.integer()) if "linear terms in all constraints" in r.comment() else []
    infinite = r.number()                             # value for infinity

    # c_l / c_u stay None when the writer omitted them (no constraints); the
    # bounds of the variables must be present, otherwise the instance encodes
    # its variable types through their absence and cannot be transcoded.
    defaults: dict[str, float] = {"c_l": None, "c_u": None, "x_l": None, "x_u": None,
                                  "x": 0.0, "y": 0.0, "z": 0.0}
    entries: dict[str, dict[int, float]] = {k: {} for k in
                                            ("c_l", "c_u", "x_l", "x_u", "x", "y", "z")}
    vtype_default = 0
    vtypes: dict[int, float] = {}

    # Remaining sections appear in writer-defined order; dispatch on labels so
    # absent sections (e.g. constraint bounds of a box-only instance) do not
    # shift the records that follow them.
    while r.pos < len(r.records):
        comment = r.comment()
        if "left-hand-side value" in comment:
            got = r.section(comment)
            if got is not None:
                defaults["c_l"], entries["c_l"] = got
        elif "right-hand-side value" in comment:
            got = r.section(comment)
            if got is not None:
                defaults["c_u"], entries["c_u"] = got
        elif "variable lower bound value" in comment:
            got = r.section(comment)
            if got is not None:
                defaults["x_l"], entries["x_l"] = got
        elif "variable upper bound value" in comment:
            got = r.section(comment)
            if got is not None:
                defaults["x_u"], entries["x_u"] = got
        elif "variable type" in comment:
            vtype_default = r.integer()
            vtypes = r.pairs(r.integer())
        elif "variable primal value" in comment:
            got = r.section(comment)
            if got is not None:
                defaults["x"], entries["x"] = got
        elif "constraint dual value" in comment:
            got = r.section(comment)
            if got is not None:
                defaults["y"], entries["y"] = got
        elif "variable bound dual value" in comment:
            got = r.section(comment)
            if got is not None:
                defaults["z"], entries["z"] = got
        elif "number of non-default variable names" in comment:
            for _ in range(r.integer()):
                r.fields()
        elif "number of non-default constraint names" in comment:
            for _ in range(r.integer()):
                r.fields()
            break
        else:
            raise ValueError(
                f"{path}: unsupported QPLIB record {r.records[r.pos][0]!r} "
                f"labelled {comment!r}"
            )

    if m > 0 and (defaults["c_l"] is None or defaults["c_u"] is None):
        raise ValueError(f"{path}: constraint records present but row bound sections missing")
    if defaults["x_l"] is None or defaults["x_u"] is None:
        raise ValueError(
            f"{path}: no variable bound records (instances whose variable types "
            "are implied rather than stated are not supported)"
        )

    return {
        "n": n,
        "m": m,
        "h": h_entries,
        "g": g,
        "g_default": g_default,
        "objective_offset": objective_offset,
        "a": a_entries,
        "infinite": infinite,
        "c_l": entries["c_l"],
        "c_u": entries["c_u"],
        "x_l": entries["x_l"],
        "x_u": entries["x_u"],
        "defaults": defaults,
        "vtypes": vtypes,
        "vtype_default": vtype_default,
        "points": {k: entries[k] for k in ("x", "y", "z")},
    }


def parse_qplib(path: pathlib.Path) -> dict:
    records = split_records(path)
    if len(records) < 5:
        raise ValueError(f"{path}: truncated QPLIB header")
    # The first three lines are free-text (name + provenance decorated with
    # units and punctuation); the positional body starts on line four.
    header = records[0][0].split()
    name = header[0]
    problem_class = records[1][0].split()[0]
    direction = records[2][0].split()[0].lower()
    if direction not in ("minimize", "maximize"):
        raise ValueError(f"{path}: unknown objective direction {direction!r}")
    body = records[3:]
    parsed = parse_annotated(body, path) if any(c for _, c in body) else parse_positional(body, path)
    parsed.update(name=name, problem_class=problem_class, direction=direction)
    return parsed


def parse_positional(body: list[tuple[str, str]], path: pathlib.Path) -> dict:
    # Comment-less dialect (generated instances): records are positional, so
    # every one of them must be present, in order.
    raw_lines = [data for data, _ in body]

    r = Reader(raw_lines)
    n = r.integer()
    m = r.integer()

    h_entries = r.triples(r.integer())
    g_default = r.number()             # default value for entries in g
    g = r.pairs(r.integer())
    objective_offset = r.number()      # "value of f"
    a_entries = r.triples(r.integer())
    infinite = r.number()              # "value of infinite bounds"

    def section() -> tuple[float, dict[int, float]]:
        """A QPLIB section: a default value line, then count + overrides."""
        default = r.number()
        return default, r.pairs(r.integer())

    defaults: dict[str, float] = {}
    entries: dict[str, dict[int, float]] = {}
    for key in ("c_l", "c_u", "x_l", "x_u"):
        defaults[key], entries[key] = section()
    # Variable types: default (0=continuous, 1=integer, 2=binary) + overrides.
    # Skipping this section shifts every later section by one record.
    vtype_default = r.integer()
    vtypes = r.pairs(r.integer())
    for key in ("x", "y", "z"):
        defaults[key], entries[key] = section()

    def is_count_line(line: str) -> bool:
        tokens = line.split()
        if len(tokens) != 1:
            return False
        try:
            int(float(tokens[0]))
            return True
        except ValueError:
            return False

    # Two trailing name sections (non-default variable names, constraint
    # names): a lone count line, then ``count`` (index, name) pairs. Names are
    # not propagated (indices are already canonical), but the sections must be
    # consumed so the structural check below sees a fully-read body.
    for _ in range(2):
        if r.pos >= len(r.lines) or not is_count_line(r.lines[r.pos]):
            break
        count = r.integer()
        for _ in range(count):
            r.fields()   # index, name — discarded

    if r.pos != len(r.lines):
        leftover = [l for l in r.lines[r.pos:] if l.strip()][:3]
        raise ValueError(
            "unsupported QPLIB section layout "
            f"(carries extra records, e.g. quadratic constraints): {leftover}"
        )
    return {
        "n": n,
        "m": m,
        "h": h_entries,
        "g": g,
        "g_default": g_default,
        "objective_offset": objective_offset,
        "a": a_entries,
        "infinite": infinite,
        "c_l": entries["c_l"],
        "c_u": entries["c_u"],
        "x_l": entries["x_l"],
        "x_u": entries["x_u"],
        "defaults": defaults,
        "vtypes": vtypes,
        "vtype_default": vtype_default,
        "points": {k: entries[k] for k in ("x", "y", "z")},
    }


def write_mps(instance: dict, out_path: pathlib.Path) -> None:
    n, m = instance["n"], instance["m"]
    name = instance["name"]
    var = lambda j: f"X{j:08d}"
    row = lambda i: f"C{i:08d}"

    lines: list[str] = []
    lines.append(f"* imported from QPLIB ({instance['problem_class']}, {instance['direction']})")
    lines.append("NAME          " + name)
    lines.append("OBJSENSE")
    lines.append((" MIN" if instance["direction"] == "minimize" else " MAX"))
    infinite = instance["infinite"]

    def is_infinite(value: float) -> bool:
        return abs(value) >= infinite

    # Resolve every row and column bound from the file's own defaults, so the
    # transcode never invents bounds the instance did not state.
    row_bounds = []
    for i in range(1, m + 1):
        lo = instance["c_l"].get(i, instance["defaults"]["c_l"])
        up = instance["c_u"].get(i, instance["defaults"]["c_u"])
        row_bounds.append((lo, up))
    var_bounds = []
    for j in range(1, n + 1):
        lo = instance["x_l"].get(j, instance["defaults"]["x_l"])
        up = instance["x_u"].get(j, instance["defaults"]["x_u"])
        var_bounds.append((lo, up))

    lines.append("ROWS")
    lines.append(" N  OBJ")
    row_types: list[str] = []
    for i in range(1, m + 1):
        lo, up = row_bounds[i - 1]
        free_lo, free_up = is_infinite(lo), is_infinite(up)
        if free_lo and free_up:
            kind = "N"   # free row: no restriction (carries no rhs/range)
        elif free_lo:
            kind = "L"
        elif free_up:
            kind = "G"
        else:
            kind = "L"
        row_types.append(kind)
        lines.append(f" {kind}  {row(i)}")

    lines.append("COLUMNS")
    g_default = instance.get("g_default", 0.0)
    vtypes = instance.get("vtypes", {})
    vtype_default = instance.get("vtype_default", 0)
    in_intorg = False
    # Group A entries by column once — a per-column scan of the full list is
    # O(n * annz) and pathological on large instances.
    a_by_col: dict[int, list[tuple[int, float]]] = {}
    for i, j, v in instance["a"]:
        a_by_col.setdefault(j, []).append((i, v))
    for j in range(1, n + 1):
        # MARKER blocks carry integrality; emit them around integer columns
        # (QPLIB: 0=continuous, 1=integer, 2=binary — binary is integer with
        # [0,1] bounds, which the BOUNDS section below fixes up).
        is_int = vtypes.get(j, vtype_default) != 0
        if is_int and not in_intorg:
            lines.append(f"    MARKER                 'MARKER'                 'INTORG'")
            in_intorg = True
        elif not is_int and in_intorg:
            lines.append(f"    MARKER                 'MARKER'                 'INTEND'")
            in_intorg = False
        value = instance["g"].get(j, g_default)
        column_entries = a_by_col.get(j, ())
        if value != 0.0:
            lines.append(f"    {var(j)}  OBJ  {value:.17g}")
        for i, v in column_entries:  # already sorted by row in file order
            lines.append(f"    {var(j)}  {row(i)}  {v:.17g}")
        if value == 0.0 and not column_entries:
            # COLUMNS is the only record that declares a variable: a column
            # that is empty everywhere else still needs a (zero) objective
            # entry, otherwise it would not exist in the emitted model.
            lines.append(f"    {var(j)}  OBJ  0.0")
    if in_intorg:
        lines.append(f"    MARKER                 'MARKER'                 'INTEND'")
    if instance["h"]:
        lines.append("QUADOBJ")
        for i, j, v in instance["h"]:
            # instance["h"] holds one triangle of the symmetric Hessian and
            # QUADOBJ expects the upper triangle of the quadratic term (the
            # reader mirrors off-diagonal entries to (j, i)), so order the
            # pair — the product is symmetric either way.
            lo, hi = (i, j) if i <= j else (j, i)
            lines.append(f"    {var(lo)}  {var(hi)}  {v:.17g}")

    # RHS carries the single row bound; RANGES carries the span for two-sided
    # rows (an L row with rhs = up and range = up - lo encodes [lo, up]; a zero
    # range encodes an equality).
    rhs_lines: list[tuple[str, float]] = []
    ranges: list[tuple[str, float]] = []
    for i in range(1, m + 1):
        lo, up = row_bounds[i - 1]
        free_lo, free_up = is_infinite(lo), is_infinite(up)
        if free_lo and free_up:
            continue
        if free_lo:
            rhs_lines.append((row(i), up))
        elif free_up:
            rhs_lines.append((row(i), lo))
        else:
            rhs_lines.append((row(i), up))
            if lo != up:
                ranges.append((row(i), up - lo))
            else:
                # lo == up is an equality row: an L row with a *zero* range
                # encodes [up - 0, up] exactly (CPLEX RANGES convention).
                ranges.append((row(i), 0.0))
    if instance["objective_offset"]:
        # CPLEX convention: an RHS entry on the objective row is the constant.
        rhs_lines.append(("OBJ", instance["objective_offset"]))
    if rhs_lines:
        lines.append("RHS")
        for rname, value in rhs_lines:
            lines.append(f"    RHS1  {rname}  {value:.17g}")
    if ranges:
        lines.append("RANGES")
        for rname, span in ranges:
            lines.append(f"    RNG1  {rname}  {span:.17g}")

    bounds_lines = []
    for j in range(1, n + 1):
        lo, up = var_bounds[j - 1]
        free_lo, free_up = is_infinite(lo), is_infinite(up)
        lo_value = -INF if free_lo else lo
        up_value = INF if free_up else up
        if lo_value == up_value:
            bounds_lines.append(f" FX BND  {var(j)}  {lo_value:.17g}")
            continue
        if lo_value != 0.0:
            bounds_lines.append(
                f" MI BND  {var(j)}" if lo_value == -INF else f" LO BND  {var(j)}  {lo_value:.17g}"
            )
        if up_value != INF:
            bounds_lines.append(f" UP BND  {var(j)}  {up_value:.17g}")
    if bounds_lines:
        lines.append("BOUNDS")
        lines.extend(bounds_lines)

    lines.append("ENDATA")
    out_path.write_text("\n".join(lines) + "\n")


def objective_at(instance: dict, x: list[float]) -> float:
    """Evaluate 1/2 x^T H x + g^T x + c at ``x`` (1-based instance indices)."""
    value = instance["objective_offset"]
    g = instance["g"]
    g_default = instance.get("g_default", 0.0)
    for j in range(1, instance["n"] + 1):
        value += g.get(j, g_default) * x[j - 1]
    for i, j, v in instance["h"]:
        # instance["h"] holds one triangle of the symmetric Hessian, so an
        # off-diagonal entry stands for both (i, j) and (j, i).  For official
        # QPLIB files the parse-time normalisation above already applied the
        # lower-left triangle rule of 1/2 x^T Q0 x.
        weight = v if i == j else 2.0 * v
        value += 0.5 * weight * x[i - 1] * x[j - 1]
    return value


def solution_value(instance: dict, sol_path: pathlib.Path) -> float:
    """Objective of an official solution file, verified against its objvar.

    QPLIB solution files list ``objvar <value>`` first and then ``x<k> <value>``
    records for the variables that are not zero (k is 1-based and may run one
    past ``n``).  Re-evaluating the objective here checks the emitted model's
    convention — triangle, off-diagonal weight, linear part and constant —
    against QPLIB's own number before it is published as a reference.
    """
    n = instance["n"]
    target: float | None = None
    points: list[tuple[int, float]] = []
    for line in sol_path.read_text().splitlines():
        fields = line.split()
        if not fields:
            continue
        if len(fields) != 2:
            raise ValueError(f"{sol_path}: unexpected record {line!r}")
        key, raw = fields
        if key == "objvar":
            target = float(raw)
        elif key.startswith("x") and key[1:].isdigit():
            points.append((int(key[1:]), float(raw)))
        else:
            raise ValueError(f"{sol_path}: unexpected record {line!r}")
    if target is None:
        raise ValueError(f"{sol_path}: no objvar record")
    if not points:
        raise ValueError(f"{sol_path}: no variable records")
    offsets = [o for o in (0, 1) if all(1 <= i - o <= n for i, _ in points)]
    if not offsets:
        raise ValueError(f"{sol_path}: variables outside 1..{n}")
    tolerance = 1e-6 * max(1.0, abs(target))
    for offset in offsets:
        x = [0.0] * n
        for i, raw in points:
            x[i - offset - 1] = raw
        value = objective_at(instance, x)
        if abs(value - target) <= tolerance:
            return target
    raise ValueError(
        f"{sol_path}: objective of the solution point disagrees with objvar "
        f"{target!r}"
    )


def import_one(source: pathlib.Path, out_dir: pathlib.Path, sol_dir: pathlib.Path | None = None) -> dict:
    if source.suffix == ".zip" or source.name.endswith(".qplib.zip"):
        with zipfile.ZipFile(source) as archive:
            members = [m for m in archive.namelist() if m.endswith(".qplib")]
            if not members:
                raise ValueError(f"{source}: no .qplib member")
            data = archive.read(members[0])
            inner_name = members[0]
    else:
        data = source.read_bytes()
        inner_name = source.name
    tmp = out_dir / "_tmp.qplib"
    tmp.write_bytes(data)
    try:
        instance = parse_qplib(tmp)
    finally:
        tmp.unlink()
    mps_path = out_dir / f"{instance['name']}.mps"
    write_mps(instance, mps_path)
    sol_path = sol_dir / f"{instance['name']}.sol" if sol_dir else None
    has_solution = sol_path is not None and sol_path.is_file()
    reference = solution_value(instance, sol_path) if has_solution else None
    provenance = {
        "source": str(source),
        "member": inner_name,
        "sha256": hashlib.sha256(data).hexdigest(),
        "name": instance["name"],
        "problem_class": instance["problem_class"],
        "direction": instance["direction"],
        "rows": instance["m"],
        "columns": instance["n"],
        "quadratic_nonzeros": len(instance["h"]),
        "linear_constraint_nonzeros": len(instance["a"]),
        "reference_objective": reference,
        "reference_solution": str(sol_path) if has_solution else None,
    }
    (out_dir / f"{instance['name']}.provenance.json").write_text(
        json.dumps(provenance, indent=2) + "\n"
    )
    return provenance


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", nargs="+", required=True, help=".qplib.zip files (or a directory)")
    parser.add_argument("--out", required=True, help="output directory for .mps + provenance")
    parser.add_argument(
        "--sols",
        default=None,
        help="directory of official QPLIB solution files (default: <out>/sol)",
    )
    args = parser.parse_args()

    inputs: list[pathlib.Path] = []
    for item in args.input:
        path = pathlib.Path(item)
        if path.is_dir():
            inputs.extend(sorted(path.glob("*.qplib.zip")))
            inputs.extend(sorted(path.glob("*.qplib")))
        else:
            inputs.append(path)
    if not inputs:
        print("no QPLIB sources found", file=sys.stderr)
        return 2

    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    sol_dir = pathlib.Path(args.sols) if args.sols else out_dir / "sol"
    for source in inputs:
        try:
            info = import_one(source, out_dir, sol_dir)
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"[-] {source}: {exc}", file=sys.stderr)
            continue
        ref = info["reference_objective"]
        marker = " [solution verified]" if info["reference_solution"] else ""
        print(
            f"[+] {info['name']}: {info['rows']}x{info['columns']} "
            f"({info['problem_class']}, {info['direction']}), "
            f"H-nnz={info['quadratic_nonzeros']}, "
            f"reference={('%.10g' % ref) if ref is not None else 'none'}{marker}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
