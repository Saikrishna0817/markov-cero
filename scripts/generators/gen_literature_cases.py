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
from scripts.support.gen_literature_cases_gen_pochet_lot_sizing import main
from scripts.support.gen_literature_cases_config import (
    REPO, _ENTRY_POINT, _EntryPath, argparse, json, os
)

if __name__ == "__main__":
    raise SystemExit(main())
