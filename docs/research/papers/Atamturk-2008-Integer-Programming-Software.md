---
type: paper
title: "Integer-Programming Software Systems"
authors: "Atamturk & Savelsbergh"
year: 2008
venue: "Ann. OR"
doi: "(unverified)"
domain: [presolve, milp]
priority: ○
status: standard
tags: [paper, presolve]
---
# Integer-Programming Software Systems
> How presolve integrates with cuts, heuristics and node LP solves inside branch-and-cut software.
## Metadata
| Field | Value |
|---|---|
| Authors | Atamturk & Savelsbergh |
| Year | 2008 |
| Venue | Ann. OR |
| DOI/URL | (unverified) |
## Problem Addressed
Papers present presolve, cuts and search as separate topics, but production systems interleave them: presolve at the root and at each node, results reused by separation and heuristics, everything reversed at postsolve. The paper describes that integration.
## Core Contribution
- **Methodology:** Architectural account of IP software systems (MINTO/COIN-era): presolve as a service invoked by the tree manager; propagation used by branching and heuristics; consistency requirements across components.
- **Assumptions:** Branch-and-cut framework with reversible transformations.
- **Benchmarks/datasets:** Reported experiences on MIP test sets (qualitative).
- **Metrics:** Overall solve performance; reduction reuse.
- **Key results:** Integration matters as much as individual rules (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut with presolve/propagation services.
**Techniques:** Model-status bookkeeping; transformation stack shared by node LP and cut generation.
**Implementation details:** Mirrors our structure: `src/milp/milp_solver.cpp` owns the tree; presolve currently runs once at root — node-level presolve (per-node bound tightening) is a natural next step and must respect `presolve_stack` reversal.
**Limitations/failure cases:** Per-node presolve costs time; must be budgeted (skip when LP solve is cheap).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 (presolve + branch-and-cut in one breath) and R18 (extensible architecture). Direct guidance for where presolve hooks belong in `src/milp/milp_solver.cpp`.
## Evidence → Engineering Decision
- *Finding:* Presolve belongs inside the tree manager, not just before it → *PS requirement:* R5 → *Component:* src/milp/milp_solver.cpp (node-entry presolve hook) → *Metric:* nodes explored vs. presolve overhead.
## Related Papers
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
## Uses
- [[Presolve]]
- [[Branch and Cut]]
