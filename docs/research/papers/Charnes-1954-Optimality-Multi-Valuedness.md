---
type: paper
title: "Optimality and Multi-Valuedness in LP (perturbation method)"
authors: "Charnes, 1954; Megiddo (1983)"
year: 1954
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Optimality and Multi-Valuedness in LP (perturbation method)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Perturb b and c slightly to destroy degeneracy, then take limits — Charnes' 1954 idea with Megiddo's polynomial ε-bound.

## Metadata
| Field | Value |
|---|---|
| Authors | Charnes, 1954; Megiddo, "On the Perturbation Method for Avoiding Degeneracy", 1983 |
| Year | 1954 (first); 1983 for the Megiddo contribution |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified; Megiddo PDF link in list) |

## Problem Addressed
Degenerate LP vertices make the simplex method stall: zero-length pivots, non-unique duals, unreliable sensitivity and unreliable branching information. Pivoting rules (Bland) fix cycling but not the underlying multi-valuedness.

## Core Contribution
- **Methodology:** Replace the LP by a perturbed one (b + εδ, c + εδ') with small ε, solve it — the perturbed problem is nondegenerate with probability 1 — and use its solution as a proxy, or take ε → 0 limits for sensitivity. Megiddo shows the method is polynomial for suitable perturbations.
- **Assumptions:** Perturbation direction chosen so the limit exists and remains optimal for the original problem; ε small enough not to change the optimum's value materially.
- **Benchmarks/datasets:** Theory; degenerate LP examples.
- **Metrics:** ε bound needed for nondegeneracy; number of degenerate pivots avoided.
- **Key results:** Provides a *principled* alternative to tie-breaking rules: change the problem, not the pivot choice. Charnes 1954 is the origin of the whole idea.

## Engineering-Relevant Knowledge
**Algorithms:** Perturbation-based anti-degeneracy; lexicographic/multi-objective tie-breaking.
**Techniques:** Small deterministic perturbation of RHS/cost; limit computation; tie-breaking rows.
**Implementation details:** Our LP path uses **Bland anti-cycling only** (`src/lp/reference/revised_simplex.cpp`) and no perturbation — R13 explicitly requires degeneracy robustness, so a tiny lexicographic perturbation (or explicit degeneracy detection) is a candidate upgrade in `src/lp/dual/dual_simplex.cpp`.
**Equations/rules:** Perturbed objective c(ε) = c + ε d with generic d; solve sequence; original optimum recovered as ε → 0⁺.
**Limitations/failure cases:** Perturbation changes reported objective slightly (tolerance must be reported); cannot be inverted for infeasible/unbounded cases without care; costlier than a pricing rule.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Degenerate models are named twice in the PS (R13, R17); perturbation is the standard systematic answer and sits inside our LP engine, not in the MILP layer.

## Evidence → Engineering Decision
- *Finding:* only Bland anti-cycling, no anti-degeneracy perturbation → *PS requirement:* R9, R13, R17 → *Component:* src/lp/dual/dual_simplex.cpp, src/lp/reference/revised_simplex.cpp → *Metric:* degenerate pivot count, [[Degeneracy]] solve rate

## Related Papers
- [[Gill-1989-Practical-Anti-Cycling]]
- [[Maros-0000-New-Degeneracy-Method]]
- [[DeFarias-2019-Positive-Edge-Pricing]]
- [[Berthold-2013-Cloud-Branching]]

## Uses
- [[Degeneracy]] [[Dual Simplex]] [[Bland Anti-Cycling]] [[Numerical Stability]]
