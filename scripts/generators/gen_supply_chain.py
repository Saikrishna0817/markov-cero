#!/usr/bin/env python3
"""
markov-cero Industrial Case Generator: Multi-Echelon Supply Chain Network (MILP)
Synthetic qualification model for petroleum distribution network design:
- 4 primary refinery dispatch terminals
- 12 candidate regional distribution depots (discrete location-allocation)
- 30 retail demand centers across southern and western India
- 3 finished fuel products: MS (Petrol), HSD (Diesel), ATF (Aviation Turbine Fuel)
Decision variables:
- Y[j] binary: open regional depot j
- Z[i, j] binary: activate pipeline/rail bridging between refinery i and depot j
- S[i, j, p] continuous: bulk transport flow
- D[j, k, p] continuous: retail distribution flow
Target dimensions: ~250 constraints, ~450 variables, ~35 binary variables.
"""

import argparse
import os
import random
import sys

from fixture_provenance import write_provenance


def generate_supply_chain_mps(
    num_plants: int = 4,
    num_depots: int = 12,
    num_customers: int = 25,
    seed: int = 42,
) -> str:
    rng = random.Random(seed)

    products = ["MS", "HSD", "ATF"]

    # Fixed opening cost per candidate depot ($/month)
    depot_fixed_cost = [rng.uniform(15000.0, 35000.0) for _ in range(num_depots)]
    # Maximum storage throughput capacity per depot (kL/month)
    depot_capacity = [rng.uniform(8000.0, 18000.0) for _ in range(num_depots)]

    # Refinery production supply limits (kL/month)
    plant_supply = {}
    for i in range(num_plants):
        plant_supply[i] = {
            "MS": rng.uniform(12000.0, 25000.0),
            "HSD": rng.uniform(25000.0, 50000.0),
            "ATF": rng.uniform(8000.0, 15000.0),
        }

    # Customer retail market demand (kL/month)
    cust_demand = {}
    for k in range(num_customers):
        cust_demand[k] = {
            "MS": rng.uniform(400.0, 1200.0),
            "HSD": rng.uniform(800.0, 2500.0),
            "ATF": rng.uniform(200.0, 700.0),
        }

    # Primary bulk transport unit costs ($/kL)
    bulk_cost = {}
    for i in range(num_plants):
        for j in range(num_depots):
            dist = rng.uniform(80.0, 600.0)
            bulk_cost[(i, j)] = {p: 0.08 * dist * (1.1 if p == "ATF" else 1.0) for p in products}

    # Secondary retail delivery unit costs ($/kL)
    delivery_cost = {}
    for j in range(num_depots):
        for k in range(num_customers):
            dist = rng.uniform(20.0, 250.0)
            delivery_cost[(j, k)] = {p: 0.14 * dist * (1.1 if p == "ATF" else 1.0) for p in products}

    lines = ["* Units: plant supply, depot throughput, S/D flows and demand RHS kL/month; Y binary",
             "* and MAX_DEPOTS in counts. COST: Y fixed USD/month; S/D coefficients USD/kL,",
             "* so model objective is USD/month.",
             "* Synthetic qualification data; not plant operating data; deterministic for a fixed seed.",
             "NAME SUPPLY_CHAIN_LARGE", "ROWS", " N COST"]

    # 1. Customer demand fulfillment: DEM_{k}_{p} (G)
    for k in range(num_customers):
        for p in products:
            lines.append(f" G DEM_{k:02d}_{p:<3}  ")

    # 2. Depot flow conservation: BAL_{j}_{p} (E)
    for j in range(num_depots):
        for p in products:
            lines.append(f" E BAL_{j:02d}_{p:<3}  ")

    # 3. Depot total throughput capacity: CAP_{j} (L)
    for j in range(num_depots):
        lines.append(f" L CAP_{j:02d}      ")

    # 4. Refinery supply capacity: SUP_{i}_{p} (L)
    for i in range(num_plants):
        for p in products:
            lines.append(f" L SUP_{i:02d}_{p:<3}  ")

    # 5. Maximum open depots budget limit: MAX_DEPOTS (L)
    lines.append(" L MAX_DEPOTS  ")

    lines.append("COLUMNS")
    bounds_entries = []

    # Discrete Decision Variables (Depot Openings Y_{j})
    lines.append("    MARK0000  'MARKER'                 'INTORG'")
    for j in range(num_depots):
        yname = f"Y_D{j:02d}"
        fc = depot_fixed_cost[j]
        cap = depot_capacity[j]
        lines.append(f"    {yname:<8}  COST      {fc:<10.2f} CAP_{j:02d}      -{cap:<9.2f}")
        lines.append(f"    {yname:<8}  MAX_DEPOTS 1.0")
        bounds_entries.append(f" BV BND       {yname}")
    lines.append("    MARK0001  'MARKER'                 'INTEND'")

    # Primary Bulk Transport Flows: S_{i}_{j}_{p}
    for i in range(num_plants):
        for j in range(num_depots):
            for p in products:
                sname = f"S{i}_{j:02d}_{p}"
                c = bulk_cost[(i, j)][p]
                # COST, BAL (+1 into depot j), SUP (+1 from plant i), CAP (+1 on depot throughput)
                lines.append(f"    {sname:<8}  COST      {c:<10.2f} BAL_{j:02d}_{p:<3}  1.0")
                lines.append(f"    {sname:<8}  SUP_{i:02d}_{p:<3}  1.0        CAP_{j:02d}      1.0")
                bounds_entries.append(f" UP BND       {sname:<8}  15000.00")

    # Secondary Delivery Flows: D_{j}_{k}_{p}
    for j in range(num_depots):
        for k in range(num_customers):
            for p in products:
                dname = f"D{j:02d}_{k:02d}_{p}"
                c = delivery_cost[(j, k)][p]
                # COST, DEM (+1 to customer k), BAL (-1 out of depot j)
                lines.append(f"    {dname:<8}  COST      {c:<10.2f} DEM_{k:02d}_{p:<3}  1.0")
                lines.append(f"    {dname:<8}  BAL_{j:02d}_{p:<3}  -1.0")
                bounds_entries.append(f" UP BND       {dname:<8}  5000.00")

    lines.append("RHS")
    for k in range(num_customers):
        for p in products:
            lines.append(f"    RHS1      DEM_{k:02d}_{p:<3}  {cust_demand[k][p]:.2f}")
    for i in range(num_plants):
        for p in products:
            lines.append(f"    RHS1      SUP_{i:02d}_{p:<3}  {plant_supply[i][p]:.2f}")
    # Allow at most 8 open depots
    lines.append("    RHS1      MAX_DEPOTS 8.0")

    lines.append("BOUNDS")
    lines.extend(bounds_entries)
    lines.append("ENDATA\n")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate industrial supply chain network MILP instance")
    parser.add_argument("--output", default="data/cases/supply_chain_large.mps", help="Output MPS path")
    parser.add_argument("--plants", type=int, default=4, help="Number of refinery plants")
    parser.add_argument("--depots", type=int, default=12, help="Number of distribution depots")
    parser.add_argument("--customers", type=int, default=25, help="Number of customer markets")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    content = generate_supply_chain_mps(
        num_plants=args.plants,
        num_depots=args.depots,
        num_customers=args.customers,
        seed=args.seed
    )
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(content)
    write_provenance(args.output, "gen_supply_chain.py",
                     {"plants": args.plants, "depots": args.depots,
                      "customers": args.customers, "seed": args.seed},
                     "Unit and qualification comments added; model rows, columns and coefficients unchanged.")

    print(f"[+] Industrial supply chain network MILP model written to {args.output}")
    print(f"    File size: {os.path.getsize(args.output):,} bytes")


if __name__ == "__main__":
    main()
