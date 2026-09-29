#!/usr/bin/env python3
"""
markov-cero Industrial Case Generator: Chemical Process Network & Hydrogen Pinch (MILP)
Synthetic qualification model for a multi-unit refinery conversion complex:
CDU, VDU, CCR, FCC, DHDT, HCU, HGU, SRU across multiple operating shifts.
Includes discrete unit mode selection, intermediate inter-unit routing,
and high-pressure hydrogen network pinch balances.
"""

import argparse
import os
import random
import sys

from fixture_provenance import write_provenance


def generate_process_network_mps(
    num_shifts: int = 7,
    seed: int = 42,
) -> str:
    rng = random.Random(seed)

    units = ["CDU", "VDU", "CCR", "FCC", "DHDT", "HCU", "HGU", "SRU"]
    modes = ["LOW", "BASE", "HIGH"]

    # Maximum feed capacity per unit (kbpd or MMSCFD for HGU)
    unit_cap = {
        "CDU": 300.0, "VDU": 140.0, "CCR": 45.0, "FCC": 65.0,
        "DHDT": 75.0, "HCU": 50.0, "HGU": 90.0, "SRU": 40.0
    }

    # Minimum feed when unit is running
    unit_min = {u: 0.3 * unit_cap[u] for u in units}

    # Unit mode operating cost ($/bbl or $/MSCF)
    mode_cost = {
        "CDU": [1.2, 1.5, 2.0], "VDU": [1.0, 1.3, 1.8],
        "CCR": [3.5, 4.2, 5.5], "FCC": [2.8, 3.4, 4.5],
        "DHDT": [2.0, 2.5, 3.2], "HCU": [3.8, 4.5, 5.8],
        "HGU": [1.5, 1.8, 2.4], "SRU": [0.8, 1.0, 1.4],
    }

    # Hydrogen yields (Nm3 H2 per bbl feed; positive = produce, negative = consume)
    h2_yields = {
        "CDU": [0.0, 0.0, 0.0],
        "VDU": [0.0, 0.0, 0.0],
        "CCR": [18.0, 22.0, 26.0],     # Reformer produces high purity H2
        "FCC": [0.0, 0.0, 0.0],
        "DHDT": [-8.0, -10.0, -14.0],  # Desulfurizer consumes H2
        "HCU": [-25.0, -32.0, -42.0],  # Hydrocracker consumes significant H2
        "HGU": [50.0, 60.0, 70.0],     # SMR Hydrogen generation
        "SRU": [0.0, 0.0, 0.0],
    }

    lines = ["* Units: liquid F feeds and capacity RHS kbpd; HGU F and RHS MMSCFD; Y modes binary.",
             "* COST coefficients are synthetic score per feed-unit and score per active mode; H2_BAL",
             "* uses normalized hydrogen-proxy units, not a physical hydrogen balance.",
             "* Synthetic qualification data; not plant operating data; deterministic for a fixed seed.",
             "NAME PROCESS_NETWORK_LARGE", "ROWS", " N COST"]

    # Rows:
    # 1. Mode exclusivity: MODE_EXCL_{unit}_{t}  (E) sum(y) = 1
    # 2. Feed min bound: FEED_MIN_{unit}_{t}     (G) Feed - Min * y >= 0
    # 3. Feed max bound: FEED_MAX_{unit}_{t}     (L) Feed - Max * y <= 0
    # 4. CDU - VDU link: VDU_FEED_{t}            (L) VDU_Feed <= 0.45 * CDU_Feed
    # 5. Hydrogen network pinch balance: H2_PINCH_{t} (G) sum(H2_prod) + sum(H2_cons) >= H2_min_reserve
    # 6. Sulfur recovery balance: SRU_FEED_{t}   (G) SRU_Feed - 0.05 * DHDT_Feed >= 0
    # 7. Refinery fuel gas balance: FG_BAL_{t}   (G)
    for t in range(num_shifts):
        for u in units:
            lines.append(f" E MEX_{u}_{t}")
            for m_idx, m in enumerate(modes):
                lines.append(f" G FMIN_{u}_{m}_{t}")
                lines.append(f" L FMAX_{u}_{m}_{t}")
        lines.append(f" L VDU_LNK_{t}")
        lines.append(f" G H2_BAL_{t}")
        lines.append(f" G SRU_BAL_{t}")
        lines.append(f" G FG_BAL_{t}")

    lines.append("COLUMNS")
    rhs_entries = []
    bounds_entries = []

    # Integer block for discrete mode indicators: Y_{unit}_{mode}_{t}
    lines.append("    MARK0000  'MARKER'                 'INTORG'")
    for t in range(num_shifts):
        for u in units:
            for m_idx, m in enumerate(modes):
                vname = f"Y_{u[:3]}_{m[:1]}_{t}"
                cap = unit_cap[u]
                min_feed = unit_min[u]
                # Mode change / operating fixed cost
                fixed_cost = 50.0 + 10.0 * m_idx
                lines.append(f"    {vname:<14} COST      {fixed_cost:<10.2f} MEX_{u}_{t} 1.0")
                lines.append(f"    {vname:<14} FMIN_{u}_{m}_{t} -{min_feed:<9.2f} FMAX_{u}_{m}_{t} -{cap:<9.2f}")
                bounds_entries.append(f" BV BND       {vname}")
    lines.append("    MARK0001  'MARKER'                 'INTEND'")

    # Continuous feed rate variables: F_{unit}_{mode}_{t}
    for t in range(num_shifts):
        for u in units:
            for m_idx, m in enumerate(modes):
                vname = f"F_{u[:3]}_{m[:1]}_{t}"
                c = mode_cost[u][m_idx]
                h2 = h2_yields[u][m_idx]

                lines.append(f"    {vname:<14} COST      {c:<10.2f} FMIN_{u}_{m}_{t} 1.0")
                lines.append(f"    {vname:<14} FMAX_{u}_{m}_{t} 1.0")

                if u == "CDU":
                    lines.append(f"    {vname:<14} VDU_LNK_{t} -0.45")
                elif u == "VDU":
                    lines.append(f"    {vname:<14} VDU_LNK_{t} 1.0")

                if abs(h2) > 1e-4:
                    lines.append(f"    {vname:<14} H2_BAL_{t} {h2:<10.2f}")

                if u == "DHDT":
                    lines.append(f"    {vname:<14} SRU_BAL_{t} -0.05")
                elif u == "SRU":
                    lines.append(f"    {vname:<14} SRU_BAL_{t} 1.0")

                # Fuel gas credit
                fg_yield = 0.08 if u in ["CCR", "FCC"] else 0.02
                lines.append(f"    {vname:<14} FG_BAL_{t} {fg_yield:<10.4f}")

                bounds_entries.append(f" UP BND       {vname:<14} {unit_cap[u]:.2f}")

    # RHS definitions
    for t in range(num_shifts):
        for u in units:
            rhs_entries.append((f"MEX_{u}_{t}", 1.0))
        rhs_entries.append((f"H2_BAL_{t}", 50.0))    # Hydrogen reserve requirement
        rhs_entries.append((f"FG_BAL_{t}", 10.0))    # Fuel gas minimum production

    lines.append("RHS")
    for row, val in rhs_entries:
        if abs(val) > 1e-6:
            lines.append(f"    RHS1      {row:<20} {val:.4f}")

    lines.append("BOUNDS")
    lines.extend(bounds_entries)
    lines.append("ENDATA\n")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate full-scale chemical process network MILP instance")
    parser.add_argument("--output", default="data/cases/process_network_large.mps", help="Output MPS path")
    parser.add_argument("--shifts", type=int, default=7, help="Number of operational shifts")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    mps_content = generate_process_network_mps(num_shifts=args.shifts, seed=args.seed)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(mps_content)
    write_provenance(args.output, "gen_process_network.py",
                     {"shifts": args.shifts, "seed": args.seed},
                     "Unit and qualification comments refreshed; model rows, columns and coefficients unchanged.")

    print(f"[+] Chemical process network MILP model written to {args.output}")
    print(f"    File size: {os.path.getsize(args.output):,} bytes")


if __name__ == "__main__":
    main()
