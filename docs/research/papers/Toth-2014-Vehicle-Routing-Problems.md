---
type: paper
title: "Vehicle Routing: Problems, Methods, and Applications"
authors: "Toth & Vigo; Gouveia, Pires & Martin"
year: 2014
venue: "Springer / MOS-SIAM (book)"
doi: "(unverified)"
domain: [domain]
priority: ○
status: standard
tags: [paper, domain]
---
# Vehicle Routing: Problems, Methods, and Applications
> The reference survey of vehicle routing — the transportation/logistics model family of R11 — plus network arc-flow formulations that stress LP relaxations.
## Metadata
| Field | Value |
|---|---|
| Authors | Toth & Vigo (2014); Gouveia, Pires & Martin (Networks, 1998) |
| Year | 2014 (first entry; no ★/✦/○ marker in source list; classified ○) |
| Venue | Springer / MOS-SIAM Series on Optimization (book) |
| DOI/URL | (unverified) — Errata note: domain-application entries still need DOI lookups |

## Problem Addressed
Routing problems (VRP variants) pair combinatorial feasibility (capacity, time windows, depots) with network-flow structure. The survey organizes problem variants and exact/heuristic solution methods; the network formulations show how formulation strength determines whether branch-and-cut can close the gap.
## Core Contribution
- **Methodology:** Taxonomy of VRP variants with exact algorithms (branch-and-cut/price, DP) and heuristics; arc-flow/multicommodity network formulations (Gouveia–Pires–Martin) with layered/graph-based reformulations for tighter relaxations.
- **Assumptions:** Metric or near-metric distances; capacity/time constraints as the core; solvers able to handle many binary arc variables.
- **Benchmarks/datasets:** Classic VRP instance sets (CVRP/X instances; not re-verified).
- **Metrics:** Gap to optimum, nodes explored, route cost quality under time limits.
- **Key results:** Tight arc-flow formulations dramatically reduce tree size versus naive formulations (qualitative restatement; figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut for routing; layered network reformulations.
**Techniques:** Tight extended formulations (many variables, strong LP relaxation) over compact weak ones; separation of routing-specific cuts.
**Implementation details:** Routing models are large and sparse — a good stress test for `src/milp/cut_pool.cpp` and `src/transform/sparse_canonicalize.cpp` once Netlib-class instances are handled.
**Limitations/failure cases:** Extended formulations trade variables for strength; memory and LP cost grow — our solver has no experience at that scale yet.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Transportation/logistics is in R11's scope; the formulation lessons (tightness beats clever branching) directly inform how we will present industrial MILP capability.
## Evidence → Engineering Decision
- *Finding:* Largest solved MILP instances are tiny (STEIN9/STEIN15/FLUGPL) → *PS requirement:* R12 → *Component:* src/milp/cut_pool.cpp → *Metric:* [[Relative Optimality Gap]] on routing-scale sparse MILP
## Related Papers
- [[Kallrath-2002-Planning-Scheduling-Industry]]
- [[Pochet-2006-Production-Planning-Mixed]]
## Uses
- [[Branch and Cut]]
