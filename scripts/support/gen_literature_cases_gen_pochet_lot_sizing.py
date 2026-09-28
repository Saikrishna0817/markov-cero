from .gen_literature_cases_config import (
    REPO, argparse, os
)
from .gen_literature_cases_gen_capitanescu_dc_opf import gen_capitanescu_dc_opf
from .gen_literature_cases_gen_capitanescu_dc_opf import gen_li_crude_blending
from .gen_literature_cases_gen_neiro_refinery import gen_neiro_refinery
from .gen_literature_cases_gen_capitanescu_dc_opf import gen_shapiro_network_flow
from .gen_literature_cases_gen_neiro_refinery import provenance
from .gen_literature_cases_gen_neiro_refinery import write_mps

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
