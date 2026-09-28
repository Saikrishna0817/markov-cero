---
type: paper
title: "Branch-and-Cut for Combinatorial Optimization"
authors: "Junger, Reinelt & Rinaldi"
year: 1995
venue: "Handbook (as listed)"
doi: "(unverified)"
domain: [milp]
priority: ○
status: standard
tags: [paper, milp]
---
# Branch-and-Cut for Combinatorial Optimization
> Handbook survey of branch-and-cut algorithmic structure — the canonical decomposition of the method.
## Metadata
| Field | Value |
|---|---|
| Authors | Junger, Reinelt & Rinaldi |
| Year | 1995 |
| Venue | Handbook (as listed) |
| DOI/URL | (unverified) |
## Problem Addressed
Branch-and-cut combines tools from combinatorial optimization, LP and polyhedral theory; the handbook chapter organizes the method into a clean structure (bounding, separation, branching, heuristics) for reference use.
## Core Contribution
- **Methodology:** Structured exposition of branch-and-cut: relaxation, separation, branching strategies, incumbent heuristics, bounding; case studies (TSP, matching, routing).
- **Assumptions:** Combinatorial problems with LP/polyhedral relaxations and separation oracles.
- **Benchmarks/datasets:** Classical combinatorial instances (qualitative).
- **Metrics:** Nodes, cuts, time.
- **Key results:** Survey/structure rather than new results (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut pipeline definition.
**Techniques:** Separation heuristics; combinatorial bounding.
**Implementation details:** Useful as the architectural vocabulary for `src/milp/` module docs: relaxation, separation, branching, incumbents — matches our file layout (node_lp, cut_pool, branch_selector, heuristics).
**Limitations/failure cases:** Handbook-level; lacks implementation tolerances.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R5 architecture reference; R18 (transparent foundation) benefits from standard terminology when documenting `src/milp/`.
## Evidence → Engineering Decision
- *Finding:* Named pipeline stages map 1:1 onto our MILP modules → *PS requirement:* R5, R18 → *Component:* src/milp/ (node_lp, cut_pool, branch_selector, heuristics) → *Metric:* per-stage timing in solve telemetry.
## Related Papers
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Cornuejols-2001-Branch-and-Cut-Algorithms]]
## Uses
- [[Branch and Cut]]
- [[LP Relaxation]]
