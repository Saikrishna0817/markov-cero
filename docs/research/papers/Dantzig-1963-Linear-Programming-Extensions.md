---
type: paper
title: "Linear Programming and Extensions / Computational Algorithm of the Revised Simplex Method"
authors: "Dantzig"
year: 1963
venue: "Book / RAND RM-1266"
doi: "(unverified)"
domain: [lp]
priority: ★
status: deep
tags: [paper, lp]
---

# Linear Programming and Extensions / Computational Algorithm of the Revised Simplex Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The origin of the simplex method and the revised simplex algorithm every modern LP engine descends from.

## Metadata
| Field | Value |
|---|---|
| Authors | Dantzig |
| Year | 1963 (book; RAND RM-1266 report dated 1953 — list gives "1953/1963") |
| Venue | Book / RAND RM-1266 |
| DOI/URL | (unverified) |

## Problem Addressed
Before 1947 there was no systematic method for solving the large linear optimization models appearing in planning, logistics and economics; Dantzig created the simplex algorithm and, in the revised form (RAND RM-1266), the computational scheme that avoids carrying the full tableau — the direct ancestor of all production simplex codes.

## Core Contribution
- **Methodology:** Simplex method with primal/dual forms, Phase I/II feasibility restoration, duality theory, and the revised simplex that maintains only the basis and prices rather than the full tableau.
- **Assumptions:** Standard-form LP; finite nondegeneracy for clean termination arguments; arithmetic treated ideally (approximate — later papers supply the numerics).
- **Benchmarks/datasets:** Applied planning/defense models of the 1950s (approximate; no instance list asserted).
- **Metrics:** Iterations, pivot counts (qualitative for the era).
- **Key results:** A practical algorithm that solves LPs to optimality by moving along vertex edges, plus the theoretical link between primal and dual optima (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex (primal), dual simplex, Phase I, pricing and ratio-test structure.

**Techniques:** Basis representation, reduced-cost pricing, ratio-test bound selection, infeasibility certificates from dual variables.

**Implementation details:** src/lp/reference/revised_simplex.cpp is a direct descendant: keep B (not the tableau), compute c̄ = c − c_B B⁻¹A by pricing solves, choose entering/leaving by the textbook rules; worked examples from the text double as unit-test vectors.

**Equations/rules:** Basic solution Bx_B = b, x_N = 0; optimality iff reduced costs c̄ⱼ ≥ 0 (minimization) (Reduced Cost); duality bound cᵀx = yᵀb (Duality Gap).

**Limitations/failure cases:** No sparse/numerical engineering, no anti-cycling, no degeneracy tolerance handling — every hardening layer is a later paper in this audit (Harris, Bartels, Koberstein).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R4 requires revised simplex and R10 requires building from the mathematical foundation; this is the primary source for the algorithm we must reimplement correctly before optimizing it.

## Evidence → Engineering Decision
- *Finding:* The revised form (basis + prices only) is the correct computational structure, not the tableau → *PS requirement:* R4 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* KKT Residual
- *Finding:* Duality provides a free optimality certificate for verification → *PS requirement:* R17 → *Component:* src/verify/ → *Metric:* KKT Residual

## Related Papers
- [[Dantzig-1954-Product-Form-Inverse]]
- [[Harris-1973-Pivot-Selection-Methods]]
- [[Vanderbei-1996-Foundations-and-Extensions]]
- [[Bixby-2002-Evolution-of-LP]]

## Uses
- [[Revised Simplex]] [[Basic Solution]] [[Duality Gap]]
