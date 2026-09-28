from __future__ import annotations
from .import_qplib_config import (
    pathlib
)
from .import_qplib_LabelReader import LabelReader
from .import_qplib_LabelReader import Reader
from .import_qplib_LabelReader import split_records

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
