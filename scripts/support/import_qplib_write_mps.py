from __future__ import annotations
from .import_qplib_config import (
    INF, pathlib
)

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
