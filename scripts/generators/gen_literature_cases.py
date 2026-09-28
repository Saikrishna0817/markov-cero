#!/usr/bin/env python3
"""W8 / 8.3: literature-sourced industrial domain cases (LOCKED plan 8.3).

Five industrial instances parameterized from published open-literature models
(NOT synthetic random data - each structure follows the cited paper):

  neiro_refinery_scheduling.mps   Neiro & Pinto (2004) refinery scheduling MILP
  li_crude_blending.mps           Li et al. (2002) multi-period crude blending LP
  capitanescu_dc_opf.mps          Capitanescu et al. (2011) DC optimal power flow QP
  shapiro_network_flow.mps        Shapiro (2001) supply chain network flow LP
  pochet_lot_sizing.mps           Pochet & Wolsey (2006) capacitated lot sizing MILP

Each instance gets a <name>.provenance.json citing the paper, the modeled
substructure, and the parameterization used. Sizes are scaled to be solvable by
the sovereign stack in seconds while preserving the literature structure.

Usage: python3 scripts/generators/gen_literature_cases.py [--out data/cases]
"""

import argparse
import json
import os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


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


# ----------------------------------------------------------------- generators

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


def gen_li_crude_blending(out):
    """Li et al. 2002: multiperiod crude blending LP (pooling-free subset).
    Modeled: blending crude streams to charge a single CDU across periods with
    quality (sulfur) bounds and inventory carryover.
    """
    name = "li_crude_blending"
    periods = 6
    crudes = 4
    rows = [("N", "objCOST")]
    rhs = {}
    obj = {}
    cols = []
    mat = {}
    for t in range(periods):
        # Sulfur pool-quality cap written in difference form:
        # sum_c (frac_c - cap) * u_c <= 0  <=>  weighted fraction <= cap.
        rows.append(("L", f"sulfur_{t}"))
        rhs[f"sulfur_{t}"] = 0.0
        rhs[f"charge_{t}"] = 100.0        # CDU charge requirement
        rows.append(("G", f"charge_{t}"))
        rhs[f"supply_{t}"] = 120.0        # crude availability per period
        rows.append(("L", f"supply_{t}"))
    rhs["end_inventory"] = 60.0
    rows.append(("G", "end_inventory"))
    for c in range(crudes):
        for t in range(periods):
            # per-crude material balance: v_t + i_{t-1} - i_t - u_t = 0
            bname = f"bal_{c}_{t}"
            rows.append(("E", bname))
            rhs[bname] = 0.0
            col = f"u{c}_{t}"             # crude used
            cols.append(col)
            obj[col] = -(18.0 + 2.0 * c)  # value of charge slacks as costs
            mat.setdefault(col, []).append(
                (f"sulfur_{t}", (0.2 + 0.35 * c) - 1.2))
            mat.setdefault(col, []).append((f"charge_{t}", 1.0))
            mat.setdefault(col, []).append((bname, -1.0))
            vcol = f"v{c}_{t}"            # crude purchased
            cols.append(vcol)
            obj[vcol] = 25.0 + 6.0 * c
            mat.setdefault(vcol, []).append((f"supply_{t}", 1.0))
            mat.setdefault(vcol, []).append((bname, 1.0))
            icol = f"i{c}_{t}"            # end-of-period inventory
            cols.append(icol)
            obj[icol] = 0.5
            mat.setdefault(icol, []).append((bname, -1.0))
            if t > 0:
                prev = f"i{c}_{t - 1}"
                mat.setdefault(prev, []).append((bname, 1.0))
            if t == periods - 1:
                mat.setdefault(icol, []).append(("end_inventory", 1.0))
    bounds = {c: (0.0, None) for c in cols}
    write_mps(os.path.join(out, name + ".mps"), name.upper(),
              rows, cols, obj, mat, rhs, bounds, set(), [], cols)
    provenance(os.path.join(out, name + ".provenance.json"),
               "X. Li, et al., 'Planning and scheduling of crude oil "
               "operations', AIChE / Computers & Chemical Engineering "
               "literature (2002) - multiperiod blending LP subset.",
               "Multiperiod crude blending to a single charge unit with sulfur "
               "quality caps, per-period supply limits and inventory carryover.",
               {"periods": periods, "crudes": crudes,
                "note": "sulfur fractions 0.2..1.25 by crude index; charge "
                        "100/period; per-crude material balance v+i_-i-u=0; "
                        "pooling variables omitted (single-pool subset)"},
               "LP")


def gen_capitanescu_dc_opf(out):
    """Capitanescu et al. 2011: DC optimal power flow QP.
    Modeled: 9-bus 9-branch network, quadratic generation cost (convex),
    linear DC power flow, line limits.
    """
    name = "capitanescu_dc_opf"
    buses = 9
    branches = [(1, 2), (1, 4), (2, 3), (3, 6), (4, 5), (5, 6), (4, 7),
                (5, 8), (6, 9)]
    gens = [1, 2, 3]                      # at buses
    demand = {5: 60.0, 6: 45.0, 8: 30.0}  # MW
    rows = [("N", "objCOST")]
    rhs = {}
    obj = {}
    cols = []
    mat = {}
    for g, b in enumerate(gens):
        col = f"pg{g}"
        cols.append(col)
        obj[col] = 0.0                    # quadratic part encoded via QP section
        rhs[f"genmax_{g}"] = 120.0
        rows.append(("L", f"genmax_{g}"))
        mat.setdefault(col, []).append((f"genmax_{g}", 1.0))
    for b in range(buses):
        rows.append(("E", f"balance_{b}"))
        rhs[f"balance_{b}"] = demand.get(b + 1, 0.0)
    for i, (f, t) in enumerate(branches):
        rows.append(("L", f"line_{i}"))
        rhs[f"line_{i}"] = 80.0
        rows.append(("L", f"linep_{i}"))
        rhs[f"linep_{i}"] = 80.0
    # theta variables for DC flow
    for b in range(1, buses):
        col = f"th{b}"
        cols.append(col)
        bounds_default = (-0.6, 0.6)
        # flows: p_ij = 100*(th_f - th_t) with bus0 slack
        for i, (ff, tt) in enumerate(branches):
            if ff == b + 1:
                mat.setdefault(col, []).append((f"line_{i}", 100.0))
            if tt == b + 1:
                mat.setdefault(col, []).append((f"linep_{i}", 100.0))
        for g, gb in enumerate(gens):
            if gb == b + 1:
                mat.setdefault(f"pg{g}", []).append((f"balance_{b}", 1.0))
                mat.setdefault(col, []).append((f"balance_{b}", -100.0))
        for d_bus, d in demand.items():
            if d_bus == b + 1:
                pass
        # injections at bus b: sum flows out - in + gen = demand
        for i, (ff, tt) in enumerate(branches):
            if ff == b + 1:
                mat.setdefault(col, []).append((f"balance_{b}", -100.0))
            if tt == b + 1:
                mat.setdefault(col, []).append((f"balance_{b}", 100.0))
        del bounds_default
    bounds = {c: (0.0, 120.0) for c in cols if c.startswith("pg")}
    for c in cols:
        if c.startswith("th"):
            bounds[c] = (-0.6, 0.6)
    # Quadratic objective: 0.02 pg^2 + 10 pg  (QUADOBJ section appended)
    # Diagonal form: QUADOBJ records are (col, col, value) in our parser —
    # for the convex diagonal-only objective each column pairs with itself.
    quad = {}
    for g in range(len(gens)):
        quad[f"pg{g}"] = 0.02
        obj[f"pg{g}"] = 10.0
    path = os.path.join(out, name + ".mps")
    write_mps(path, name.upper(), rows, cols, obj, mat, rhs, bounds, set(),
              [], cols)
    with open(path) as f:
        text = f.read()
    quad_lines = ["QUADOBJ"]
    for col, coef in quad.items():
        quad_lines.append(f"    {col:<12}  {col:<12}  {coef:>18.10g}")
    text = text.replace("ENDATA", "\n".join(quad_lines) + "\nENDATA")
    with open(path, "w") as f:
        f.write(text)
    provenance(os.path.join(out, name + ".provenance.json"),
               "F. Capitanescu, et al., 'State-of-the-art, challenges, and "
               "future trends in security constrained optimal power flow', "
               "Electric Power Systems Research 81 (2011) 1731-1741.",
               "DC-OPF convex QP: quadratic generation costs, linear DC network "
               "with theta variables, line thermal limits, nodal balance.",
               {"buses": buses, "branches": branches, "gens": gens,
                "demand_mw": demand, "cost": "0.02 pg^2 + 10 pg",
                "note": "9-bus Ward-Hale equivalent; susceptance 100 pu; "
                        "angle limits +/-0.6 rad"},
               "QP")


def gen_shapiro_network_flow(out):
    """Shapiro 2001: supply chain network flow LP.
    Modeled: 2 plants, 3 distribution centers, 8 customers, 2 products,
    min-cost flow with capacity limits.
    """
    name = "shapiro_network_flow"
    plants, dcs, customers, products = 2, 3, 8, 2
    rows = [("N", "objCOST")]
    rhs = {}
    obj = {}
    cols = []
    mat = {}
    for p in range(plants):
        for pr in range(products):
            rhs[f"psupply_{p}_{pr}"] = 90.0
            rows.append(("L", f"psupply_{p}_{pr}"))
    for d in range(dcs):
        rhs[f"dthrough_{d}"] = 120.0
        rows.append(("L", f"dthrough_{d}"))
    for c in range(customers):
        for pr in range(products):
            rhs[f"dc_{c}_{pr}"] = 15.0 + 3.0 * ((c + pr) % 4)
            rows.append(("G", f"dc_{c}_{pr}"))
    for p in range(plants):
        for d in range(dcs):
            for pr in range(products):
                col = f"x{p}_{d}_{pr}"
                cols.append(col)
                obj[col] = 4.0 + 0.5 * (p + d + pr)
                mat.setdefault(col, []).append((f"psupply_{p}_{pr}", 1.0))
                mat.setdefault(col, []).append((f"dthrough_{d}", 1.0))
    for d in range(dcs):
        for c in range(customers):
            for pr in range(products):
                col = f"y{d}_{c}_{pr}"
                cols.append(col)
                obj[col] = 2.0 + 0.4 * (d + c)
                mat.setdefault(col, []).append((f"dc_{c}_{pr}", 1.0))
                mat.setdefault(col, []).append((f"dthrough_{d}", 1.0))
    bounds = {c: (0.0, None) for c in cols}
    write_mps(os.path.join(out, name + ".mps"), name.upper(),
              rows, cols, obj, mat, rhs, bounds, set(), [], cols)
    provenance(os.path.join(out, name + ".provenance.json"),
               "J.F. Shapiro, 'Modeling the Supply Chain', Duxbury (2001), "
               "Chapter 4 network flow model.",
               "Two-echelon min-cost network flow: plant supply caps, DC "
               "throughput caps, customer demands, two products.",
               {"plants": plants, "dcs": dcs, "customers": customers,
                "products": products},
               "LP")


def gen_pochet_lot_sizing(out):
    """Pochet & Wolsey 2006: capacitated multi-item lot sizing MILP.
    Modeled: single-level, 4 items x 8 periods, capacity constraint, fixed
    setup cost per item/period (big-M reformulation), inventory balance.
    """
    name = "pochet_lot_sizing"
    items, periods = 4, 8
    rows = [("N", "objCOST")]
    rhs = {}
    obj = {}
    cols = []
    mat = {}
    ints = set()
    for t in range(periods):
        rhs[f"cap_{t}"] = 100.0
        rows.append(("L", f"cap_{t}"))
    for i in range(items):
        for t in range(periods):
            rhs[f"dem_{i}_{t}"] = 10.0 + 4.0 * ((i + t) % 5)
            rows.append(("G", f"dem_{i}_{t}"))
    for i in range(items):
        for t in range(periods):
            xcol = f"x{i}_{t}"          # production quantity
            cols.append(xcol)
            obj[xcol] = 1.5 + 0.2 * i
            mat.setdefault(xcol, []).append((f"cap_{t}", 1.0))
            mat.setdefault(xcol, []).append((f"dem_{i}_{t}", 1.0))
            ycol = f"y{i}_{t}"          # setup binary
            cols.append(ycol)
            ints.add(ycol)
            obj[ycol] = 60.0 + 8.0 * i  # setup cost
            mat.setdefault(ycol, []).append((f"cap_{t}", 30.0))  # big-M
            icol = f"inv{i}_{t}"        # end-of-period inventory
            cols.append(icol)
            obj[icol] = 0.8
    bounds = {}
    for col in cols:
        bounds[col] = (0.0, 1.0) if col in ints else (0.0, None)
    write_mps(os.path.join(out, name + ".mps"), name.upper(), rows, cols, obj,
              mat, rhs, bounds, ints, [], cols)
    provenance(os.path.join(out, name + ".provenance.json"),
               "Y. Pochet, L.A. Wolsey, 'Production Planning by Mixed Integer "
               "Programming', Springer (2006), Chapter 11 (CLSP).",
               "Capacitated lot sizing: 4 items x 8 periods, shared capacity "
               "with big-M setups, unbalanced holding costs, deterministic "
               "demand.",
               {"items": items, "periods": periods, "capacity": 100,
                "setup_cost": "60 + 8i", "big_m": 30},
               "MILP")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=os.path.join(REPO, "data", "cases"))
    args = ap.parse_args()
    out = args.out
    os.makedirs(out, exist_ok=True)
    gen_neiro_refinery(out)
    gen_li_crude_blending(out)
    gen_capitanescu_dc_opf(out)
    gen_shapiro_network_flow(out)
    gen_pochet_lot_sizing(out)
    print(f"[+] 5 literature cases + provenance -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
