#!/usr/bin/env python3
"""
markov-cero Industrial Case Generator: Crude & Component Blending (Convex QP)
Synthetic multi-component refinery crude/intermediate blending qualification case.
Follows the canonical formulation of examples/cases/crude_oil_blending.mps scaled
to industrial dimensions:
- N component feedstocks/crudes (e.g., Arab Light, Maya, Bonny Light, Basrah, Murban, etc.)
- M product allocation demands (Motor Spirit, High Speed Diesel, Aviation Turbine Fuel, Fuel Oil)
- Quality specifications: Octane giveaway, Sulfur compliance, Cetane index, Density, Viscosity
- Quadratic penalty / giveaway cost matrix in QUADOBJ
Target dimensions: 100 - 300 variables, 50 - 150 constraints, strictly convex QP.
"""

import argparse
import os
import random
import sys

from fixture_provenance import write_provenance


def generate_crude_blending_qps(
    num_components: int = 120,
    seed: int = 42,
) -> str:
    rng = random.Random(seed)

    # 8 Finished Product Demands
    products = [
        ("MS91", 85.0),      # Euro-VI Gasoline 91 RON
        ("MS95", 45.0),      # Premium Gasoline 95 RON
        ("HSD_BS6", 210.0),  # Ultra-low sulfur diesel (BS-VI)
        ("JET_A1", 75.0),    # Aviation Turbine Fuel
        ("MGO", 35.0),       # Marine Gasoil
        ("FO180", 50.0),     # Low Viscosity Fuel Oil
        ("FO380", 65.0),     # High Viscosity Fuel Oil
        ("BITUMEN", 30.0),   # Paving Bitumen VG-30
    ]

    # Quality constraint definitions: (name, sense, threshold)
    # Modeled as deviation balance rows with RHS = 0
    quality_specs = [
        ("MS_OCTANE", "G"),    # Octane balance for Gasoline pool
        ("MS_SULFUR", "L"),    # Sulfur balance for Gasoline pool (max 10 ppm)
        ("MS_BENZENE", "L"),   # Benzene limit in Gasoline
        ("MS_RVP", "L"),       # Reid Vapor Pressure limit
        ("HSD_CETANE", "G"),   # Cetane number for Diesel pool (min 51)
        ("HSD_SULFUR", "L"),   # Sulfur in Diesel (max 10 ppm)
        ("HSD_DENSITY", "L"),  # Density limit in Diesel (max 845 kg/m3)
        ("HSD_POUR", "L"),     # Pour point for cold flow
        ("JET_FREEZE", "L"),   # Freezing point for Aviation Jet (max -47 C)
        ("JET_SMOKE", "G"),    # Smoke point for Jet (min 25 mm)
        ("FO_VISC", "L"),      # Viscosity upper bound for Fuel Oil
        ("FO_SULFUR", "L"),    # Sulfur upper bound for Marine Fuel
    ]

    lines = ["* Units: X flows and product/availability RHS synthetic volume units (SVU); COST linear",
             "* coefficients synthetic currency/SVU; QUADOBJ synthetic currency/SVU^2; quality rows",
             "* SVU times normalized dimensionless deviation, not physical specification equations.",
             "* Synthetic qualification data; not plant operating data; deterministic for a fixed seed.",
             "NAME CRUDE_BLENDING_LARGE", "ROWS", " N COST"]

    # Product demand rows
    for p_name, _ in products:
        lines.append(f" G {p_name:<10}")

    # Quality specification deviation rows
    for q_name, sense in quality_specs:
        lines.append(f" {sense} {q_name:<10}")

    # Storage availability bounds for each component
    for i in range(num_components):
        lines.append(f" L SAVL_{i:03d}  ")

    lines.append("COLUMNS")
    bounds_entries = []
    quad_entries = []

    for i in range(num_components):
        vname = f"X{i:03d}"
        cat = i % 4  # 0: Light Naphtha, 1: Kero/Distillate, 2: Gasoil, 3: Heavy Residue

        # Base procurement cost ($/bbl)
        base_cost = 45.0 + 5.0 * cat + rng.uniform(-3.0, 3.0)
        # Availability limit
        avail = rng.uniform(40.0, 120.0)

        # Product split fractions (which products this component can realistically yield)
        prod_yields = {}
        if cat == 0:  # Naphtha / Gasoline pool
            prod_yields["MS91"] = rng.uniform(0.40, 0.70)
            prod_yields["MS95"] = rng.uniform(0.20, 0.40)
        elif cat == 1:  # Kerosene / Jet / Distillate
            prod_yields["JET_A1"] = rng.uniform(0.35, 0.65)
            prod_yields["HSD_BS6"] = rng.uniform(0.20, 0.40)
        elif cat == 2:  # Gasoil / Diesel
            prod_yields["HSD_BS6"] = rng.uniform(0.50, 0.80)
            prod_yields["MGO"] = rng.uniform(0.15, 0.30)
        else:  # Heavy cuts / Fuel oil / Bitumen
            prod_yields["FO180"] = rng.uniform(0.20, 0.40)
            prod_yields["FO380"] = rng.uniform(0.30, 0.50)
            prod_yields["BITUMEN"] = rng.uniform(0.10, 0.25)

        # Quality deviation coefficients (normalized to [-3.0, +3.0])
        qual_devs = {}
        if cat == 0:
            qual_devs["MS_OCTANE"] = rng.uniform(0.5, 3.0)
            qual_devs["MS_SULFUR"] = rng.uniform(-2.0, -0.5)
            qual_devs["MS_BENZENE"] = rng.uniform(-1.0, 1.0)
            qual_devs["MS_RVP"] = rng.uniform(-1.5, 1.5)
        elif cat == 1:
            qual_devs["JET_FREEZE"] = rng.uniform(-2.5, -0.5)
            qual_devs["JET_SMOKE"] = rng.uniform(0.5, 2.5)
            qual_devs["HSD_CETANE"] = rng.uniform(0.2, 1.5)
        elif cat == 2:
            qual_devs["HSD_CETANE"] = rng.uniform(0.5, 2.5)
            qual_devs["HSD_SULFUR"] = rng.uniform(-2.5, -0.5)
            qual_devs["HSD_DENSITY"] = rng.uniform(-1.5, 1.0)
            qual_devs["HSD_POUR"] = rng.uniform(-2.0, 0.5)
        else:
            qual_devs["FO_VISC"] = rng.uniform(-1.5, 1.5)
            qual_devs["FO_SULFUR"] = rng.uniform(-2.0, 1.0)

        # Emit COLUMNS entries (max 2 entries per line)
        col_pairs = [("COST", base_cost), (f"SAVL_{i:03d}", 1.0)]
        for p_name, y_val in prod_yields.items():
            col_pairs.append((p_name, y_val))
        for q_name, q_val in qual_devs.items():
            col_pairs.append((q_name, q_val))

        for idx in range(0, len(col_pairs), 2):
            r1, v1 = col_pairs[idx]
            if idx + 1 < len(col_pairs):
                r2, v2 = col_pairs[idx + 1]
                lines.append(f"    {vname:<8}  {r1:<10} {v1:<10.4f} {r2:<10} {v2:<10.4f}")
            else:
                lines.append(f"    {vname:<8}  {r1:<10} {v1:<10.4f}")

        bounds_entries.append(f" UP BND       {vname:<8}  {avail:.2f}")

        # Strictly positive quadratic giveaway / quality deviation cost
        quad_coeff = rng.uniform(0.5, 2.0)
        quad_entries.append((vname, vname, quad_coeff))

    lines.append("RHS")
    for p_name, p_dem in products:
        lines.append(f"    RHS1      {p_name:<10} {p_dem:.4f}")
    for i in range(num_components):
        avail = rng.uniform(40.0, 120.0)
        lines.append(f"    RHS1      SAVL_{i:03d}   {avail:.4f}")

    lines.append("BOUNDS")
    lines.extend(bounds_entries)

    lines.append("QUADOBJ")
    for v1, v2, qval in quad_entries:
        lines.append(f"    {v1:<8}  {v2:<8}  {qval:.4f}")

    lines.append("ENDATA\n")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate industrial crude blending Convex QP instance")
    parser.add_argument("--output", default="data/cases/crude_blending_large.qps", help="Output QPS path")
    parser.add_argument("--components", type=int, default=120, help="Number of component feedstocks")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    content = generate_crude_blending_qps(num_components=args.components, seed=args.seed)
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(content)
    write_provenance(args.output, "gen_crude_blending.py",
                     {"components": args.components, "seed": args.seed},
                     "Unit and qualification comments refreshed; model rows, columns and coefficients unchanged.")

    print(f"[+] Industrial crude blending Convex QP model written to {args.output}")
    print(f"    File size: {os.path.getsize(args.output):,} bytes")


if __name__ == "__main__":
    main()
