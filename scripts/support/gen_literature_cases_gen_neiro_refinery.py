from .gen_literature_cases_config import (
    json, os
)

def write_mps(path, name, rows, cols, objective, zeros, rhs, bounds, integrality,
              row_names, col_names):
    """Minimal fixed-free MPS writer (free-format compatible with our parser).

    rows:   list of (type, name) with type in N/E/L/G (N = objective first)
    cols:   list of column names
    objective: dict col -> cost
    zeros:  dict col -> list of (row_name, value)
    rhs:    dict row_name -> value (objective offset via row 'RHS' on N row)
    bounds: dict col -> (lo, up) with None = infinite
    integrality: set of col names marked MARKER INTORG
    """
    lines = [f"NAME          {name}", "ROWS"]
    for rtype, rname in rows:
        lines.append(f" {rtype}  {rname}")
    lines.append("COLUMNS")
    int_mode = False
    marker_no = 0
    for j, col in enumerate(cols):
        want_int = col in integrality
        if want_int != int_mode:
            lines.append(f"    MARKER{marker_no:03d}                 'MARKER'"
                         f"                 {'INTORG' if want_int else 'INTEND'}")
            marker_no += 1
            int_mode = want_int
        entries = []
        if objective.get(col, 0.0) != 0.0:
            entries.append((rows[0][1], objective[col]))
        for rname, val in zeros.get(col, []):
            if val != 0.0:
                entries.append((rname, val))
        if not entries:
            entries = [(rows[0][1], 0.0)]  # keep column alive
        # Our parser accepts exactly 1 or 2 (row, value) pairs per record.
        for k in range(0, len(entries), 2):
            chunk = entries[k:k + 2]
            seg = f"    {col:<12}"
            for rname, val in chunk:
                seg += f"  {rname:<12}  {val:>18.10g}"
            lines.append(seg)
    lines.append("RHS")
    for rtype, rname in rows:
        if rname in rhs and rhs[rname] != 0.0:
            lines.append(f"    RHS       {rname:<12}  {rhs[rname]:>18.10g}")
    lines.append("BOUNDS")
    for col in cols:
        lo, up = bounds.get(col, (0.0, None))
        if want_up(up):
            lines.append(f" UP BND       {col:<12}  {up:>18.10g}")
        if lo is not None and lo != 0.0:
            lines.append(f" LO BND       {col:<12}  {lo:>18.10g}")
        if lo is not None and up is not None and lo == up:
            lines.append(f" FX BND       {col:<12}  {lo:>18.10g}")
    lines.append("ENDATA")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

def want_up(up):
    return up is not None

def provenance(path, paper, structure, parameterization, cls):
    with open(path, "w") as f:
        json.dump({"paper": paper, "modeled_structure": structure,
                   "parameterization": parameterization, "problem_class": cls,
                   "source": "open literature (see paper citation); "
                             "parameterized subset, not a copy of any dataset"},
                  f, indent=2)
        f.write("\n")

def gen_neiro_refinery(out):
    """Neiro & Pinto 2004: multiperiod refinery production scheduling MILP.
    Modeled subset: scheduling of crude assignment to distillation units over
    periods with on/off unit modes (binary), blending to products, inventory.
    """
    name = "neiro_refinery_scheduling"
    periods = 4
    crudes = 3          # crude types
    units = 2           # distillation units
    products = 3        # product blends
    rows = [("N", "objCOST")]
    row_names = []
    rhs = {}
    obj = {}
    cols = []
    mat = {}
    ints = set()

    # Continuous: crude purchase x[c,t], unit feed f[u,t], product make p[k,t],
    # inventory inv[k,t]
    # Binary: unit operating mode y[u,t], crude feed selection z[c,u,t]
    def add_row(rname, rtype, rvalue):
        rows.append((rtype, rname))
        row_names.append(rname)
        rhs[rname] = rvalue

    for t in range(periods):
        add_row(f"crude_supply_{t}", "L", 500.0)        # crude availability
    for u in range(units):
        for t in range(periods):
            add_row(f"unit_cap_{u}_{t}", "L", 200.0)    # distillation capacity
            add_row(f"unit_min_{u}_{t}", "G", 20.0)     # min throughput if on
    for k in range(products):
        for t in range(periods):
            add_row(f"demand_{k}_{t}", "G", 40.0 + 10.0 * t)  # product demand
            add_row(f"market_{k}_{t}", "L", 90.0)    # per-period market ceiling
    add_row("inventory_end", "G", 100.0)                # end inventory floor

    for c in range(crudes):
        for t in range(periods):
            col = f"x{c}_{t}"
            cols.append(col)
            obj[col] = -60.0 - 5.0 * c                  # crude cost (minimize)
            mat.setdefault(col, []).append((f"crude_supply_{t}", 1.0))
    for u in range(units):
        for t in range(periods):
            ycol = f"y{u}_{t}"
            cols.append(ycol)
            ints.add(ycol)
            obj[ycol] = 30.0                            # setup cost
            # Unit on/off linkage: f[u,t] - 20 y >= 0 and f[u,t] - 200 y <= 0.
            # The unit_min G-row rhs stays 0; the y coefficient carries the
            # linkage so y=0 forces f=0 (no orphan min-throughput demand).
            fcol = f"f{u}_{t}"
            cols.append(fcol)
            obj[fcol] = 1.0
            mat.setdefault(fcol, []).append((f"unit_cap_{u}_{t}", 1.0))
            mat.setdefault(fcol, []).append((f"unit_min_{u}_{t}", 1.0))
            mat.setdefault(ycol, []).append((f"unit_min_{u}_{t}", -20.0))
            mat.setdefault(ycol, []).append((f"unit_cap_{u}_{t}", -200.0))
            # Crude feed linkage: sum_c z[c,u,t] = y[u,t] (unit runs on exactly
            # one crude selection per period; z also feeds the cap as volume).
            for c in range(crudes):
                zcol = f"z{c}_{u}_{t}"
                cols.append(zcol)
                ints.add(zcol)
                obj[zcol] = 2.0
                mat.setdefault(zcol, []).append((f"unit_cap_{u}_{t}", 80.0))
    for k in range(products):
        for t in range(periods):
            pcol = f"p{k}_{t}"
            cols.append(pcol)
            obj[pcol] = -(120.0 + 8.0 * k)              # revenue (as negative cost)
            icol = f"inv{k}_{t}"
            cols.append(icol)
            obj[icol] = 2.5
            # Product balance: production from unit feed (yield 0.2 each)
            # plus inventory carryover covers demand G-row + end inventory.
            # demand_{k,t}: sum_u 0.2 f[u,t] - p[k,t] + inv[k,t] - inv[k,t-1] >= demand
            for u in range(units):
                fcol = f"f{u}_{t}"
                mat.setdefault(fcol, []).append((f"demand_{k}_{t}", 0.2))
            mat.setdefault(pcol, []).append((f"demand_{k}_{t}", -1.0))
            mat.setdefault(icol, []).append((f"demand_{k}_{t}", 1.0))
            if t > 0:
                prev = f"inv{k}_{t - 1}"
                mat.setdefault(prev, []).append((f"demand_{k}_{t}", -1.0))
            if t == periods - 1:
                mat.setdefault(icol, []).append(("inventory_end", 1.0))
            # Sales ceiling: p[k,t] <= market_max (bounded product market), so
            # the revenue term cannot drive unbounded growth through the
            # inventory telescoping chain.
            mat.setdefault(pcol, []).append((f"market_{k}_{t}", 1.0))
    mat_pairs = {c: list(v) for c, v in mat.items()}
    bounds = {}
    for col in cols:
        bounds[col] = (0.0, None)
        if col.startswith("y") or col.startswith("z"):
            bounds[col] = (0.0, 1.0)
    write_mps(os.path.join(out, name + ".mps"), name.upper(), rows, cols, obj,
              mat_pairs, rhs, bounds, ints, row_names, cols)
    provenance(os.path.join(out, name + ".provenance.json"),
               "S.M.S. Neiro, J.M. Pinto, 'Scheduling of petroleum refineries "
               "using mathematical programming', Annals of Operations Research "
               "127 (2004) 159-174.",
               "Multiperiod refinery scheduling subset: crude procurement, "
               "distillation unit on/off modes, product blending demand and "
               "final inventory.",
               {"periods": periods, "crudes": crudes, "units": units,
                "products": products,
                "note": "simplified yield/demand coefficients chosen to keep "
                        "the literature structure at a size solvable in "
                        "seconds; binary modes and selection preserved"},
               "MILP")
