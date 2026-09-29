#!/usr/bin/env python3
"""Command-line entry point; reusable implementation lives in scripts/support."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.support.gen_literature_cases_gen_neiro_refinery import write_mps
from scripts.support.gen_literature_cases_gen_neiro_refinery import want_up
from scripts.support.gen_literature_cases_gen_neiro_refinery import provenance
from scripts.support.gen_literature_cases_gen_neiro_refinery import gen_neiro_refinery
from scripts.support.gen_literature_cases_gen_capitanescu_dc_opf import gen_li_crude_blending
from scripts.support.gen_literature_cases_gen_capitanescu_dc_opf import gen_capitanescu_dc_opf
from scripts.support.gen_literature_cases_gen_capitanescu_dc_opf import gen_shapiro_network_flow
from scripts.support.gen_literature_cases_gen_pochet_lot_sizing import gen_pochet_lot_sizing
from scripts.support.gen_literature_cases_config import (
    REPO, _ENTRY_POINT, _EntryPath, argparse, json, os
)
from fixture_provenance import write_provenance


# These parameterized literature subsets do not use the synthetic refinery
# Quantity schema. State their own model units without changing the matrices.
_HEADERS = {
    "neiro_refinery_scheduling": (
        "* Units: crude/feed/product/inventory quantities synthetic volume units per period;",
        "* on/off and selection variables counts; objective synthetic currency per horizon.",
    ),
    "li_crude_blending": (
        "* Units: crude purchase/use/inventory synthetic volume units per period; sulfur row",
        "* uses a dimensionless fraction difference times volume; objective synthetic currency.",
    ),
    "capitanescu_dc_opf": (
        "* Units: generation, nodal demand and line flow MW; voltage angle radians;",
        "* objective synthetic currency with quadratic generation cost.",
    ),
    "shapiro_network_flow": (
        "* Units: supply, depot/customer demand and arc flow synthetic units per period;",
        "* objective synthetic currency per period.",
    ),
    "pochet_lot_sizing": (
        "* Units: production, inventory, demand and capacity synthetic items per period;",
        "* setup variables counts; objective synthetic currency per horizon.",
    ),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default=os.path.join(REPO, "data", "cases"))
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for generator in (gen_neiro_refinery, gen_li_crude_blending,
                      gen_capitanescu_dc_opf, gen_shapiro_network_flow,
                      gen_pochet_lot_sizing):
        generator(args.out)
    for name, header in _HEADERS.items():
        model_path = _EntryPath(args.out) / (name + ".mps")
        model_path.write_text("\n".join(header) + "\n" + model_path.read_text(encoding="utf-8"),
                              encoding="utf-8")
        provenance_path = model_path.with_suffix(".provenance.json")
        existing = json.loads(provenance_path.read_text(encoding="utf-8"))
        write_provenance(model_path, "gen_literature_cases.py", {"case": name},
                         "Unit comments added; parameterized literature model unchanged.", existing)
    print(f"[+] 5 literature cases + provenance -> {args.out}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
