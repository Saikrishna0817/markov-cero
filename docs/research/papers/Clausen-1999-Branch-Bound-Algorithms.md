---
type: paper
title: "Branch and Bound Algorithms — Principles and Examples"
authors: "Clausen"
year: 1999
venue: "(not listed in source)"
doi: "(unverified)"
domain: [milp]
priority: ○
status: standard
tags: [paper, milp]
---
# Branch and Bound Algorithms — Principles and Examples
> Practical, example-driven treatment of branch-and-bound mechanics: node order, bounds, backtracking.
## Metadata
| Field | Value |
|---|---|
| Authors | Clausen |
| Year | 1999 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |
## Problem Addressed
Branch-and-bound is often taught as folklore; students implement plausible but subtly wrong search (wrong pruning sign, missing incumbent updates, no termination proof). Clausen gives worked examples and the correctness invariants.
## Core Contribution
- **Methodology:** Walkthroughs of B&B on classic problems (knapsack, assignment, TSP): bounding functions, node selection (depth/breadth/best-bound), backtracking, complexity observations.
- **Assumptions:** Admissible bounding function; finite branching.
- **Benchmarks/datasets:** Textbook instances.
- **Metrics:** Nodes; bound quality; worst-case exponential behavior demonstrated.
- **Key results:** Correctness conditions for pruning; empirical node counts (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-bound with selectable node/bounding policies.
**Techniques:** Admissible bounds; best-bound vs. depth-first trade-offs.
**Implementation details:** Good checklist for `src/milp/work_queue.cpp` (node ordering) and `src/milp/milp_solver.cpp` (prune tests); also documents why bounds must be compared with tolerances, not equality.
**Limitations/failure cases:** Weak bounds explode the tree; textbook examples ignore degenerate LPs.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R5 (branch-and-bound) — correctness reference for node selection/pruning; our advanced strategies come from other sources (Achterberg 2007 branching, node selection literature).
## Evidence → Engineering Decision
- *Finding:* Pruning with strict `>` vs. `>=` and missing tolerances causes wrong optima → *PS requirement:* R9, R17 → *Component:* src/milp/milp_solver.cpp (bound comparison with feasibility tolerance) → *Metric:* verifier-confirmed optimality on regression suite.
## Related Papers
- [[Land-1960-Automatic-Method-Solving]]
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
## Uses
- [[Branch and Bound]]
- [[Relative Optimality Gap]]
