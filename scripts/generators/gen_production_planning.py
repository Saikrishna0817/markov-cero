#!/usr/bin/env python3
"""
markov-cero Industrial Case Generator: Production Planning & Lot-Sizing (MILP)
Ground truth model for capacitated multi-item lot sizing (CLSP)
in petrochemical polymer extrusion and finished fuel packaging.
Reference: Pochet & Wolsey (2006) 'Production Planning by Mixed Integer Programming'.
Features:
- N products, L production/packaging lines, T time periods
- Setup costs and changeover binary variables Y[p, l, t]
- Production lot-sizes X[p, l, t] with line capacity and variable upper bounds
- Inventory balance with holding costs and non-zero safety stock requirements
"""

import argparse
import os
import random
import sys


def generate_production_planning_mps(
    num_products: int = 12,
    num_lines: int = 4,
    num_periods: int = 6,
    seed: int = 42,
) -> str:
    rng = random.Random(seed)

    # Line capacities per period (hours, e.g. 168 hours = 1 continuous 24/7 operating week)
    line_cap = [168.0, 168.0, 168.0, 168.0][:num_lines]
    while len(line_cap) < num_lines:
        line_cap.append(168.0)

    # Unit production time per product on line (hours/ton)
    prod_time = {}
    for p in range(num_products):
        prod_time[p] = [rng.uniform(0.10, 0.25) for _ in range(num_lines)]

    # Setup time per product on line (hours)
    setup_time = {}
    for p in range(num_products):
        setup_time[p] = [rng.uniform(3.0, 8.0) for _ in range(num_lines)]

    # Setup cost per product on line ($)
    setup_cost = {}
    for p in range(num_products):
        setup_cost[p] = [rng.uniform(150.0, 500.0) for _ in range(num_lines)]

    # Unit variable production cost ($/ton)
    var_cost = {}
    for p in range(num_products):
        var_cost[p] = [rng.uniform(35.0, 75.0) for _ in range(num_lines)]

    # Inventory holding cost per period ($/ton/period)
    holding_cost = [rng.uniform(2.0, 5.0) for _ in range(num_products)]

    # Demand per product per period (tons)
    demand = {}
    for p in range(num_products):
        demand[p] = [rng.uniform(40.0, 120.0) for _ in range(num_periods)]

    # Initial inventory (tons)
    init_inv = [rng.uniform(20.0, 50.0) for _ in range(num_products)]

    # Minimum safety stock per period (tons)
    safety_stock = [rng.uniform(10.0, 30.0) for _ in range(num_products)]

    # Maximum production batch (Big M)
    max_batch = {}
    for p in range(num_products):
        for l in range(num_lines):
            max_batch[(p, l)] = (line_cap[l] - setup_time[p][l]) / prod_time[p][l]

    lines = ["NAME PROD_PLAN_LARGE", "ROWS", " N COST"]

    # 1. Inventory balance: IB_{p}_{t} (E)
    # 2. Line capacity:     LC_{l}_{t} (L)
    # 3. Variable upper bound: VUB_{p}_{l}_{t} (L)
    # 4. Max setups per line: SL_{l}_{t} (L)
    for t in range(num_periods):
        for p in range(num_products):
            lines.append(f" E IB_{p:02d}_{t:02d}")
        for l in range(num_lines):
            lines.append(f" L LC_{l:02d}_{t:02d}")
            lines.append(f" L SL_{l:02d}_{t:02d}")
            for p in range(num_products):
                lines.append(f" L VUB_{p:02d}_{l:02d}_{t:02d}")

    lines.append("COLUMNS")
    bounds_entries = []

    # Binary Setup variables: Y{p}_{l}_{t}
    lines.append("    MARK0000  'MARKER'                 'INTORG'")
    for t in range(num_periods):
        for l in range(num_lines):
            for p in range(num_products):
                vname = f"Y{p:02d}_{l}_{t:02d}"
                sc = setup_cost[p][l]
                st = setup_time[p][l]
                m = max_batch[(p, l)]
                row_cap = f"LC_{l:02d}_{t:02d}"
                row_vub = f"VUB_{p:02d}_{l:02d}_{t:02d}"
                row_sl = f"SL_{l:02d}_{t:02d}"
                # Pair 1: COST, LC
                lines.append(f"    {vname:<14} COST      {sc:<10.2f} {row_cap:<10} {st:<10.2f}")
                # Pair 2: VUB (-M), SL (+1)
                lines.append(f"    {vname:<14} {row_vub:<10} -{m:<9.2f} {row_sl:<10} 1.0")
                bounds_entries.append(f" BV BND       {vname}")
    lines.append("    MARK0001  'MARKER'                 'INTEND'")

    # Continuous Production variables: X{p}_{l}_{t}
    for t in range(num_periods):
        for l in range(num_lines):
            for p in range(num_products):
                vname = f"X{p:02d}_{l}_{t:02d}"
                vc = var_cost[p][l]
                pt = prod_time[p][l]
                row_ib = f"IB_{p:02d}_{t:02d}"
                row_cap = f"LC_{l:02d}_{t:02d}"
                row_vub = f"VUB_{p:02d}_{l:02d}_{t:02d}"
                lines.append(f"    {vname:<14} COST      {vc:<10.2f} {row_ib:<10} 1.0")
                lines.append(f"    {vname:<14} {row_cap:<10} {pt:<10.4f} {row_vub:<10} 1.0")
                m = max_batch[(p, l)]
                bounds_entries.append(f" UP BND       {vname:<14} {m:.2f}")

    # Continuous Inventory variables: I{p}_{t}
    for p in range(num_products):
        hc = holding_cost[p]
        ss = safety_stock[p]
        for t in range(num_periods):
            vname = f"I{p:02d}_{t:02d}"
            row_curr = f"IB_{p:02d}_{t:02d}"
            if t + 1 < num_periods:
                row_next = f"IB_{p:02d}_{t+1:02d}"
                lines.append(f"    {vname:<14} COST      {hc:<10.2f} {row_curr:<10} -1.0")
                lines.append(f"    {vname:<14} {row_next:<10} 1.0")
            else:
                lines.append(f"    {vname:<14} COST      {hc:<10.2f} {row_curr:<10} -1.0")
            bounds_entries.append(f" LO BND       {vname:<14} {ss:.2f}")
            bounds_entries.append(f" UP BND       {vname:<14} 2000.0")

    # RHS definitions
    rhs_entries = []
    for t in range(num_periods):
        for p in range(num_products):
            d = demand[p][t]
            net_d = d - init_inv[p] if t == 0 else d
            rhs_entries.append((f"IB_{p:02d}_{t:02d}", net_d))
        for l in range(num_lines):
            rhs_entries.append((f"LC_{l:02d}_{t:02d}", line_cap[l]))
            rhs_entries.append((f"SL_{l:02d}_{t:02d}", 4.0))

    lines.append("RHS")
    for row, val in rhs_entries:
        if abs(val) > 1e-6:
            lines.append(f"    RHS1      {row:<14} {val:.4f}")

    lines.append("BOUNDS")
    lines.extend(bounds_entries)
    lines.append("ENDATA\n")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate full-scale production planning & lot sizing MILP instance")
    parser.add_argument("--output", default="data/cases/production_planning_large.mps", help="Output MPS path")
    parser.add_argument("--products", type=int, default=12, help="Number of distinct products")
    parser.add_argument("--lines", type=int, default=4, help="Number of production lines")
    parser.add_argument("--periods", type=int, default=6, help="Number of planning periods")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    mps_content = generate_production_planning_mps(
        num_products=args.products,
        num_lines=args.lines,
        num_periods=args.periods,
        seed=args.seed
    )
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(mps_content)

    print(f"[+] Production planning MILP model written to {args.output}")
    print(f"    File size: {os.path.getsize(args.output):,} bytes")


if __name__ == "__main__":
    main()
