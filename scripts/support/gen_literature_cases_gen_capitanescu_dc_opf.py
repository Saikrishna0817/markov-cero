from .gen_literature_cases_config import (
    os
)
from .gen_literature_cases_gen_neiro_refinery import provenance
from .gen_literature_cases_gen_neiro_refinery import write_mps

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
