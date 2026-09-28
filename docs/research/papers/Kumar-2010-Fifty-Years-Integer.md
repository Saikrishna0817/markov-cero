---
type: paper
title: "Fifty Years of Integer Programming: A Review of the Solution Approaches"
authors: "Kumar, Luhandjula, Munapo & Jones"
year: 2010
venue: "(unverified)"
doi: "10.1177/097324701000600301"
domain: [survey, milp]
priority: ○
status: standard
tags: [paper, survey, milp]
---
# Fifty Years of Integer Programming: A Review of the Solution Approaches

> Expository map of the integer-programming solution approaches developed since 1959.
## Metadata
| Field | Value |
|---|---|
| Authors | Kumar, Luhandjula, Munapo & Jones |
| Year | 2010 |
| Venue | (unverified) |
| DOI/URL | 10.1177/097324701000600301 |
## Problem Addressed
Fifty years of integer programming produced a bewildering variety of exact and heuristic techniques; a newcomer needs one account that relates cutting planes, branch-and-bound, dynamic programming, Lagrangian relaxation and metaheuristics instead of a single-algorithm tutorial.
## Core Contribution
- **Methodology:** Narrative survey organized by solution paradigm with worked conceptual examples; no new algorithm.
- **Assumptions:** Expository treatment; claims are qualitative (no experiments).
- **Benchmarks/datasets:** None (review article).
- **Metrics:** Not applicable.
- **Key results:** Classification of IP approaches and their hybridization into modern branch-and-cut (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-bound/branch-and-cut, cutting-plane methods, Lagrangian relaxation, DP-based exact methods.
**Techniques:** Relaxation-and-tighten loops, bounding, heuristic screening, decomposition.
**Implementation details:** Confirms the component set (relaxation, bounding, cuts, heuristics, branching) any MILP core must expose as separable, individually testable modules.
**Equations/rules:** IP feasibility is NP-hard; the LP relaxation supplies the natural bound for minimization (LP Relaxation).
**Limitations/failure cases:** Survey depth only — no implementation guidance, numerics or measurements.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Orientation reading that frames why our MILP core is built around LP relaxations, cuts and branching (R5) rather than exotic exact methods; it drives no code path by itself.
## Evidence → Engineering Decision
- *Finding:* Modern IP solvers are hybrids of relax–cut–branch–heuristics → *PS requirement:* R5 → *Component:* src/milp/milp_solver.cpp → *Metric:* Relative Optimality Gap
## Related Papers
- [[Achterberg-2007-Constraint-Integer-Programming]] [[Nemhauser-1988-Integer-Combinatorial-Optimization]] [[Hoffman-1991-Improving-LP-Representations]]
## Uses
- [[LP Relaxation]]
