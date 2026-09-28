---
type: paper
title: "Planning and Scheduling in Industry"
authors: "Kallrath"
year: 2002
venue: "Springer (book)"
doi: "(unverified)"
domain: [domain]
priority: ○
status: standard
tags: [paper, domain]
---
# Planning and Scheduling in Industry
> Practice-focused catalog of production planning and scheduling formulations — the MILP model patterns behind the PS's planning/logistics scope.
## Metadata
| Field | Value |
|---|---|
| Authors | Kallrath |
| Year | 2002 (no ★/✦/○ marker on this entry in source list; classified ○) |
| Venue | Springer (book) |
| DOI/URL | (unverified) — Errata note: domain-application entries still need DOI lookups |

## Problem Addressed
Industrial planning (what to make, how much, with which capacity) and scheduling (when, on which equipment) are solved with different model families that practitioners routinely conflate. The book organizes the problem classes, formulation patterns and practical pitfalls for both.
## Core Contribution
- **Methodology:** Catalog of MILP formulations: lot-sizing, capacitated planning, batch/continuous scheduling, network design; guidance on formulation strength, decomposition and solver choice.
- **Assumptions:** Deterministic data as the base case; commercial/open MIP solvers available.
- **Benchmarks/datasets:** Industrial case studies (not re-verified).
- **Metrics:** Model size, solve time, solution quality vs. practice baselines.
- **Key results:** Formulation choice dominates runtime; strong compact formulations reduce tree size more than algorithm tweaks (qualitative restatement).
## Engineering-Relevant Knowledge
**Algorithms:** Lot-sizing and scheduling MILP formulations.
**Techniques:** Time-indexed vs. event-indexed formulations; aggregation vs. disaggregation trade-offs; strong formulations over weak ones.
**Implementation details:** These patterns dictate what `src/milp/*` must sustain: long sparse constraints, big-M bounds (a [[Scaling]] and numerical-robustness hazard) and many binaries.
**Limitations/failure cases:** Weak formulations (big-M, loose capacity aggregation) produce the weak relaxations R13 calls out — solvers then depend on cuts/branching we are still building.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It supplies the formulation vocabulary for R11's planning/logistics scope and a source of realistic instances for R12/R19 case studies.
## Evidence → Engineering Decision
- *Finding:* In-repo domain instances are toys (2×2 refinery/blend models) → *PS requirement:* R11 → *Component:* src/milp/cut_pool.cpp → *Metric:* root gap closed (%) on planning-class MILP instances
## Related Papers
- [[Neiro-2004-Mathematical-Modeling-Petroleum]]
- [[Pochet-2006-Production-Planning-Mixed]]
## Uses
- [[Branch and Cut]]
