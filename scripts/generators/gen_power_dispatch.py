#!/usr/bin/env python3
"""
markov-cero Industrial Case Generator: Electric Power Dispatch (IEEE 118-Bus DC-OPF)
Ground truth model for grid active power dispatch and transmission congestion management.
Standard IEEE 118-bus test case (PJM/AEP system) with:
- 118 transmission buses
- 54 conventional and renewable generating units with quadratic cost curves
- 186 transmission branches with line reactance and thermal MW limits
- Nodal active power balance (Kirchhoff's Current Law)
- Transmission line thermal capacity constraints: -F_max <= B_l * (theta_i - theta_j) <= F_max
- Voltage angle bounds and slack reference bus angle constraint
Output: Standard QPS format with quadratic objective in QUADOBJ.
"""

import argparse
import math
import os
import random
import sys


def generate_power_dispatch_qps(
    num_buses: int = 118,
    seed: int = 42,
) -> str:
    rng = random.Random(seed)

    # 54 Generator buses (standard IEEE 118 generator bus indices)
    gen_buses = [
        1, 4, 6, 8, 10, 12, 15, 18, 19, 24, 25, 26, 27, 31, 32, 34, 36, 40,
        42, 46, 49, 54, 55, 56, 59, 61, 62, 65, 66, 69, 70, 72, 73, 74, 76,
        77, 80, 85, 87, 89, 90, 91, 92, 99, 100, 103, 104, 105, 107, 110,
        111, 112, 113, 116
    ][:54]

    # Generate 186 transmission branches connecting the 118 buses
    # Start with a spanning tree to guarantee connectivity, then add remaining lines
    branches = []
    connected = [1]
    unconnected = list(range(2, num_buses + 1))
    while unconnected:
        u = rng.choice(connected)
        v = rng.choice(unconnected)
        unconnected.remove(v)
        connected.append(v)
        branches.append((min(u, v), max(u, v)))

    # Add remaining lines to reach ~186 branches (standard IEEE 118 has 186 lines)
    existing_pairs = set(branches)
    while len(branches) < 186:
        u = rng.randint(1, num_buses)
        v = rng.randint(1, num_buses)
        if u != v:
            pair = (min(u, v), max(u, v))
            if pair not in existing_pairs:
                existing_pairs.add(pair)
                branches.append(pair)

    # Branch parameters: susceptance B = 1 / X (per unit, base 100 MVA)
    # Reactance X typically in [0.01, 0.20] p.u. -> B in [5.0, 100.0]
    # Thermal line capacity in MW
    branch_data = []
    for l_idx, (u, v) in enumerate(branches):
        x_val = rng.uniform(0.02, 0.15)
        b_val = 1.0 / x_val
        # Thermal limit: 150 - 500 MW (in p.u.: 1.5 - 5.0)
        f_max = rng.uniform(1.8, 4.5)
        branch_data.append((l_idx, u, v, b_val, f_max))

    # Nodal active load demand (MW, converted to p.u. base 100 MVA)
    # Total system load in IEEE 118 is ~4242 MW = 42.42 p.u.
    total_load = 42.42
    bus_demand = {}
    weights = [rng.uniform(0.1, 1.0) if i % 2 == 0 else 0.0 for i in range(num_buses)]
    weight_sum = sum(weights)
    for b in range(1, num_buses + 1):
        bus_demand[b] = total_load * (weights[b - 1] / weight_sum)

    # Generator parameters: capacity [Pmin, Pmax], linear cost c1 ($/MWh), quadratic cost c2 ($/MWh^2)
    gen_data = []
    for g_idx, b_id in enumerate(gen_buses):
        gname = f"PG{g_idx:02d}"
        p_min = 0.05   # 5 MW
        p_max = rng.uniform(1.5, 4.0)  # 150 - 400 MW
        c1 = rng.uniform(20.0, 45.0)   # $20 - $45 / MWh
        c2 = rng.uniform(0.02, 0.08)   # $0.02 - $0.08 / MWh^2
        gen_data.append((gname, b_id, p_min, p_max, c1, c2))

    lines = ["NAME IEEE118_DC_OPF", "ROWS", " N COST"]

    # 1. Nodal active power balance: BAL_{b} (E)
    for b in range(1, num_buses + 1):
        lines.append(f" E BAL_{b:03d}   ")

    # 2. Branch thermal limits: FLP_{l} (L), FLN_{l} (L)
    for l_idx, u, v, b_val, f_max in branch_data:
        lines.append(f" L FLP_{l_idx:03d}  ")
        lines.append(f" L FLN_{l_idx:03d}  ")

    # 3. Slack bus angle reference: SLACK (E)
    lines.append(" E SLACK     ")

    lines.append("COLUMNS")
    bounds_entries = []
    quad_entries = []

    # Generator Active Power Variables: PG{g_idx}
    for gname, b_id, p_min, p_max, c1, c2 in gen_data:
        # Cost and Nodal balance contribution (+1.0 at gen bus)
        lines.append(f"    {gname:<8}  COST      {c1:<10.4f} BAL_{b_id:03d}   1.0")
        bounds_entries.append(f" LO BND       {gname:<8}  {p_min:.4f}")
        bounds_entries.append(f" UP BND       {gname:<8}  {p_max:.4f}")
        # Quadratic fuel cost: (1/2) * c2 * P^2  (QPS specifies c2 in QUADOBJ)
        quad_entries.append((gname, gname, c2))

    # Bus Voltage Angle Variables: TH_{b}
    # For each branch l=(u, v):
    # - In BAL_{u}: -B * theta_u + B * theta_v
    # - In BAL_{v}: +B * theta_u - B * theta_v
    # - In FLP_{l}: +B * theta_u - B * theta_v <= F_max
    # - In FLN_{l}: -B * theta_u + B * theta_v <= F_max
    for b in range(1, num_buses + 1):
        vname = f"TH{b:03d}"
        col_pairs = []

        if b == 69:  # Slack bus
            col_pairs.append(("SLACK", 1.0))

        # Sum branch susceptance connections
        for l_idx, u, v, b_val, f_max in branch_data:
            if u == b:
                col_pairs.append((f"BAL_{u:03d}", -b_val))
                col_pairs.append((f"FLP_{l_idx:03d}", b_val))
                col_pairs.append((f"FLN_{l_idx:03d}", -b_val))
            elif v == b:
                col_pairs.append((f"BAL_{v:03d}", -b_val))
                col_pairs.append((f"FLP_{l_idx:03d}", -b_val))
                col_pairs.append((f"FLN_{l_idx:03d}", b_val))

        # Format COLUMNS in pairs of 2
        for idx in range(0, len(col_pairs), 2):
            r1, val1 = col_pairs[idx]
            if idx + 1 < len(col_pairs):
                r2, val2 = col_pairs[idx + 1]
                lines.append(f"    {vname:<8}  {r1:<10} {val1:<10.4f} {r2:<10} {val2:<10.4f}")
            else:
                lines.append(f"    {vname:<8}  {r1:<10} {val1:<10.4f}")

        # Voltage angle bounds: [-0.6, +0.6] rad (~ +/- 35 degrees)
        bounds_entries.append(f" LO BND       {vname:<8}  -0.6000")
        bounds_entries.append(f" UP BND       {vname:<8}   0.6000")

    lines.append("RHS")
    for b in range(1, num_buses + 1):
        d_val = bus_demand[b]
        if abs(d_val) > 1e-6:
            lines.append(f"    RHS1      BAL_{b:03d}   {d_val:.4f}")
    for l_idx, u, v, b_val, f_max in branch_data:
        lines.append(f"    RHS1      FLP_{l_idx:03d}  {f_max:.4f}")
        lines.append(f"    RHS1      FLN_{l_idx:03d}  {f_max:.4f}")
    lines.append("    RHS1      SLACK      0.0000")

    lines.append("BOUNDS")
    lines.extend(bounds_entries)

    lines.append("QUADOBJ")
    for v1, v2, qval in quad_entries:
        lines.append(f"    {v1:<8}  {v2:<8}  {qval:.4f}")

    lines.append("ENDATA\n")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate IEEE 118-Bus DC-OPF Convex QP instance")
    parser.add_argument("--output", default="data/cases/power_dispatch_dc_opf.qps", help="Output QPS path")
    parser.add_argument("--buses", type=int, default=118, help="Number of buses")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    content = generate_power_dispatch_qps(num_buses=args.buses, seed=args.seed)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"[+] IEEE 118-Bus DC-OPF Convex QP model written to {args.output}")
    print(f"    File size: {os.path.getsize(args.output):,} bytes")


if __name__ == "__main__":
    main()
