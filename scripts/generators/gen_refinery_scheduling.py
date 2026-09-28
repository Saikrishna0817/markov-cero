#!/usr/bin/env python3
"""
markov-cero Industrial Case Generator: Refinery Production Scheduling (MILP)
Ground truth model for MRPL coastal refinery scheduling over a 30-day horizon.
Includes discrete tanker arrivals, crude tank farm inventory, CDU throughputs,
fractionation yields, and multi-period fuel supply commitments.
"""

import argparse
import math
import os
import random
import sys


def generate_refinery_scheduling_mps(
    days: int = 30,
    num_crudes: int = 10,
    num_tanks: int = 12,
    num_cdus: int = 3,
    num_berths: int = 4,
    seed: int = 42,
) -> str:
    rng = random.Random(seed)

    crude_names = [
        "ARABLGT", "ARABHVY", "BASRAH", "MAYA", "BONNY",
        "MUMBAI", "KUWAIT", "MURBAN", "SOKOL", "SVERDRP"
    ][:num_crudes]

    # Cut yields for each crude: [Naphtha, Kerosene, Gasoil, VGO, Residue]
    cut_names = ["NAPH", "KERO", "GASOIL", "VGO", "RESID"]
    cut_yields = {
        "ARABLGT": [0.20, 0.15, 0.30, 0.20, 0.15],
        "ARABHVY": [0.12, 0.10, 0.25, 0.25, 0.28],
        "BASRAH":  [0.16, 0.12, 0.28, 0.24, 0.20],
        "MAYA":    [0.10, 0.08, 0.22, 0.25, 0.35],
        "BONNY":   [0.26, 0.18, 0.34, 0.14, 0.08],
        "MUMBAI":  [0.22, 0.16, 0.32, 0.18, 0.12],
        "KUWAIT":  [0.15, 0.13, 0.27, 0.23, 0.22],
        "MURBAN":  [0.24, 0.17, 0.33, 0.16, 0.10],
        "SOKOL":   [0.25, 0.16, 0.35, 0.15, 0.09],
        "SVERDRP": [0.18, 0.14, 0.30, 0.22, 0.16],
    }

    # Crude cost per barrel ($)
    crude_cost = {
        "ARABLGT": 75.0, "ARABHVY": 65.0, "BASRAH": 70.0, "MAYA": 60.0, "BONNY": 82.0,
        "MUMBAI": 76.0, "KUWAIT": 71.0, "MURBAN": 80.0, "SOKOL": 81.0, "SVERDRP": 74.0,
    }

    # Product net values ($/bbl)
    cut_value = {
        "NAPH": 78.0, "KERO": 92.0, "GASOIL": 88.0, "VGO": 70.0, "RESID": 48.0
    }

    lines = ["NAME REFINERY_SCHEDULING_LARGE", "ROWS", " N COST"]

    # 1. Tank inventory mass balances: INV_BAL_{tank}_{t}  (E)
    for k in range(num_tanks):
        for t in range(days):
            lines.append(f" E INV_BAL_{k}_{t}")

    # 2. CDU capacity constraints: CDU_CAP_{u}_{t}  (L)
    cdu_capacities = [120.0, 150.0, 180.0][:num_cdus]
    for u in range(num_cdus):
        for t in range(days):
            lines.append(f" L CDU_CAP_{u}_{t}")

    # 3. Product yield aggregation: PROD_YIELD_{cut}_{t}  (E)
    for cut in cut_names:
        for t in range(days):
            lines.append(f" E PROD_YIELD_{cut}_{t}")

    # 4. Product demand satisfaction: PROD_DEM_{cut}_{t}  (G)
    for cut in cut_names:
        for t in range(days):
            lines.append(f" G PROD_DEM_{cut}_{t}")

    # 5. Tanker berthing exclusivity: BERTH_EXCL_{berth}_{t}  (L)
    for b in range(num_berths):
        for t in range(days):
            lines.append(f" L BERTH_EXCL_{b}_{t}")

    # 6. Unload rate coupling with berthing binary: UNLD_MAX_{c}_{b}_{t}  (L)
    berth_routes = []
    for c_idx, c_name in enumerate(crude_names):
        b = c_idx % num_berths
        k = c_idx % num_tanks
        berth_routes.append((c_name, b, k))

    for c_name, b, k in berth_routes:
        for t in range(days):
            lines.append(f" L UNLD_MAX_{c_name}_{b}_{t}")

    # 7. Tanker arrival schedule windows (L)
    for c_name, b, k in berth_routes:
        for t in range(days):
            lines.append(f" L TANKER_WIN_{c_name}_{t}")

    lines.append("COLUMNS")
    rhs_entries = []
    bounds_entries = []

    # Map crude assigned to tank
    tank_crude = {k: crude_names[k % len(crude_names)] for k in range(num_tanks)}

    # Binary variables: Berthing indicators YB_{c}_{b}_{t}
    lines.append("    MARK0000  'MARKER'                 'INTORG'")
    for c_name, b, k in berth_routes:
        for t in range(days):
            vname = f"YB_{c_name[:4]}_{b}_{t}"
            lines.append(f"    {vname:<10} COST      150.0     BERTH_EXCL_{b}_{t} 1.0")
            lines.append(f"    {vname:<10} UNLD_MAX_{c_name}_{b}_{t} -80.0 TANKER_WIN_{c_name}_{t} 1.0")
            bounds_entries.append(f" BV BND       {vname}")
    lines.append("    MARK0001  'MARKER'                 'INTEND'")

    # Continuous variables:
    # 1. Unload flows QU_{c}_{b}_{t} (kbpd)
    for c_name, b, k in berth_routes:
        cost = crude_cost[c_name]
        for t in range(days):
            vname = f"QU_{c_name[:4]}_{b}_{t}"
            # Feeds into tank k: coeff -1.0 in INV_BAL_{k}_{t}
            lines.append(f"    {vname:<10} COST      {cost:<10.2f} INV_BAL_{k}_{t} -1.0")
            lines.append(f"    {vname:<10} UNLD_MAX_{c_name}_{b}_{t} 1.0")
            bounds_entries.append(f" UP BND       {vname:<10} 80.0")

    # 2. Inventory levels INV_{k}_{t}
    # INV_{k, t} - INV_{k, t-1} - QU + CG = 0
    # INV_{k, t} has +1.0 in row t, and -1.0 in row t+1
    tank_cap = 250.0
    tank_min = 10.0
    for k in range(num_tanks):
        for t in range(days):
            vname = f"INV_{k}_{t}"
            lines.append(f"    {vname:<10} COST      0.25      INV_BAL_{k}_{t} 1.0")
            if t + 1 < days:
                lines.append(f"    {vname:<10} INV_BAL_{k}_{t+1} -1.0")
            bounds_entries.append(f" LO BND       {vname:<10} {tank_min:.2f}")
            bounds_entries.append(f" UP BND       {vname:<10} {tank_cap:.2f}")

    # 3. CDU Charge flows CG_{k}_{u}_{t}
    for k in range(num_tanks):
        c_name = tank_crude[k]
        yields = cut_yields[c_name]
        for u in range(num_cdus):
            for t in range(days):
                vname = f"CG_{k}_{u}_{t}"
                lines.append(f"    {vname:<10} COST      1.50      INV_BAL_{k}_{t} 1.0")
                lines.append(f"    {vname:<10} CDU_CAP_{u}_{t} 1.0")
                for c_idx, cut in enumerate(cut_names):
                    alpha = yields[c_idx]
                    lines.append(f"    {vname:<10} PROD_YIELD_{cut}_{t} -{alpha:.4f}")
                bounds_entries.append(f" UP BND       {vname:<10} 60.0")

    # 4. Product Sales / Production PR_{cut}_{t}
    for cut in cut_names:
        price = cut_value[cut]
        for t in range(days):
            vname = f"PR_{cut}_{t}"
            lines.append(f"    {vname:<10} COST      -{price:<9.2f} PROD_YIELD_{cut}_{t} 1.0")
            lines.append(f"    {vname:<10} PROD_DEM_{cut}_{t} 1.0")
            bounds_entries.append(f" UP BND       {vname:<10} 200.0")

    # RHS definitions
    # Initial inventory at t=0
    for k in range(num_tanks):
        init_inv = 80.0
        rhs_entries.append((f"INV_BAL_{k}_0", init_inv))

    # CDU capacities
    for u in range(num_cdus):
        cap = cdu_capacities[u]
        for t in range(days):
            rhs_entries.append((f"CDU_CAP_{u}_{t}", cap))

    # Berth exclusivity: at most 2 tankers across berths per day
    for b in range(num_berths):
        for t in range(days):
            rhs_entries.append((f"BERTH_EXCL_{b}_{t}", 1.0))

    # Product minimum daily demand commitments
    demand_base = {"NAPH": 10.0, "KERO": 12.0, "GASOIL": 30.0, "VGO": 15.0, "RESID": 8.0}
    for cut in cut_names:
        base = demand_base[cut]
        for t in range(days):
            dem = base * (1.0 + 0.05 * math.sin(t * 0.2))
            rhs_entries.append((f"PROD_DEM_{cut}_{t}", dem))

    # Tanker arrival windows: tankers can berth on their designated window
    for c_name, b, k in berth_routes:
        for t in range(days):
            # Regular arrival windows
            window_open = 1.0 if ((t + (b * 2)) % 3 != 0) else 0.0
            rhs_entries.append((f"TANKER_WIN_{c_name}_{t}", window_open))

    lines.append("RHS")
    for row, val in rhs_entries:
        if abs(val) > 1e-6:
            lines.append(f"    RHS1      {row:<20} {val:.4f}")

    lines.append("BOUNDS")
    lines.extend(bounds_entries)
    lines.append("ENDATA\n")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate full-scale refinery scheduling MILP instance")
    parser.add_argument("--output", default="data/cases/refinery_scheduling_large.mps", help="Output MPS path")
    parser.add_argument("--days", type=int, default=7, help="Planning horizon in days")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    mps_content = generate_refinery_scheduling_mps(days=args.days, seed=args.seed)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(mps_content)

    print(f"[+] Refinery scheduling MILP model written to {args.output}")
    print(f"    File size: {os.path.getsize(args.output):,} bytes")


if __name__ == "__main__":
    main()
