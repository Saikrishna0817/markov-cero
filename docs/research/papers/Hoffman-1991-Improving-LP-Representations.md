---
type: paper
title: "Improving LP-Representations of Zero-One Linear Programs for Branch-and-Cut"
authors: "Hoffman & Padberg"
year: 1991
venue: "Journal of Computing"
doi: "(unverified)"
domain: [milp]
priority: ○
status: standard
tags: [paper, milp]
---

# Improving LP-Representations of Zero-One Linear Programs for Branch-and-Cut
> Tightens 0-1 LP relaxations before branch-and-cut so weak root bounds become usable.
## Metadata
| Field | Value |
|---|---|
| Authors | Hoffman & Padberg |
| Year | 1991 |
| Venue | Journal of Computing |
| DOI/URL | (unverified) |
## Problem Addressed
0-1 programs often have LP relaxations too weak for effective branch-and-cut: fractional solutions exploit missing implied bounds and covering structure, inflating tree size. The paper catalogues reformulations that strengthen the root relaxation without changing the integer hull.
## Core Contribution
- **Methodology:** Structural analysis of 0-1 constraints (covering, partitioning, cardinality, variable bounds) mapped to valid LP-strengthening transformations.
- **Assumptions:** Binary variables; constraints whose combinatorial structure is detectable syntactically or by simple inference.
- **Benchmarks/datasets:** Representative 0-1 models of the era (qualitative; no instance list asserted).
- **Metrics:** Root relaxation gap and branch-and-bound tree size (qualitative).
- **Key results:** Class-by-class tightening rules that reduce the integrality gap at the root (approximate; no figures asserted).
## Engineering-Relevant Knowledge
**Algorithms:** Implied-bound inference, bound propagation, cover/knapsack reformulation executed in presolve.
**Techniques:** Coefficient reduction, singleton/dominated-row handling, strengthening of covering inequalities.
**Implementation details:** Runs before any LP solve and must be fully reversible (postsolve map) or MIP solution recovery breaks; interacts with src/presolve/presolve.cpp and src/transform/canonicalize.cpp.
**Equations/rules:** For binaries, a ≤ b constraints imply per-variable bound updates; covering rows Σ aⱼxⱼ ≥ b with aⱼ > b give xⱼ = 1 forced for aⱼ ≥ b (bound tightening).
**Limitations/failure cases:** Strengthening can densify rows and hurt sparse factorization; over-eager reformulation risks numerical harm without changing the integer solution set.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R5 requires presolve and R12 large sparse models; tighter root relaxations shrink trees directly (R20), but each transformation must be weighed against fill-in (R6) and must keep postsolve exact.
## Evidence → Engineering Decision
- *Finding:* Tighter root relaxations shrink branch-and-bound trees → *PS requirement:* R5 → *Component:* src/presolve/presolve.cpp → *Metric:* Relative Optimality Gap
## Related Papers
- [[Achterberg-2007-Constraint-Integer-Programming]] [[Nemhauser-1988-Integer-Combinatorial-Optimization]] [[Kumar-2010-Fifty-Years-Integer]]
## Uses
- [[LP Relaxation]] [[Presolve]]
