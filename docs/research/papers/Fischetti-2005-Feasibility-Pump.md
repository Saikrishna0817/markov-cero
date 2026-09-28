---
type: paper
title: "The Feasibility Pump"
authors: "Fischetti, Glover & Lodi"
year: 2005
venue: "Math. Prog."
doi: "(unverified)"
domain: [heuristics]
priority: ★
status: deep
tags: [paper, heuristics]
---

# The Feasibility Pump

> Rounds an LP solution toward integrality by iteratively projecting back to the LP relaxation — the fastest generic route to a first incumbent.

## Metadata
| Field | Value |
|---|---|
| Authors | Fischetti, Glover & Lodi |
| Year | 2005 |
| Venue | Mathematical Programming (list: "Math. Prog.") |
| DOI/URL | (unverified; PDF link in reference list) |

## Problem Addressed
MIP solvers can spend enormous time in branch-and-bound without ever producing a feasible solution, so bounds never tighten and nothing prunes. A cheap, always-available way to manufacture an incumbent is needed.

## Core Contribution
- **Methodology:** Let x̄ be the LP relaxation solution and x* the closest integer point (rounding). Alternate: project x* back onto the LP relaxation (minimize distance to x* subject to LP constraints) and round again; restart with perturbation on failure.
- **Assumptions:** LP feasibility at every iteration (Phase-I/infeasibility handling needed), bounded integer variables for the rounding step.
- **Benchmarks/datasets:** MIPLIB instances of the era (paper reports strong first-solution performance).
- **Metrics:** Time/iterations to first feasible solution; success rate.
- **Key results:** Finds incumbents in a fraction of a second where B&B alone takes much longer; the basis of nearly every modern solver's first incumbent heuristic.

## Engineering-Relevant Knowledge
**Algorithms:** [[Feasibility Pump]] (original binary form); L1/L2 distance objectives for projection.
**Techniques:** Nearest-integer rounding; perturbation/restart when cycling; alternating projection.
**Implementation details:** We already ship a FP in `src/milp/heuristics.cpp` (paired with rounding). The original needs repeated LP solves — our [[Dual Simplex]] warm start is the right engine; each iteration is a small LP, so FP cost must be capped.
**Equations/rules:** Phase 1: x* = nearest_integer(x̄); Phase 2: x̄' = argmin ‖x − x*‖ over LP relaxation; stop when x* feasible or after k iterations (k typically 5–50).
**Limitations/failure cases:** Fails on strong coupling (tight linking constraints) because rounding violates them; no objective awareness (fixed by #112); may cycle without perturbation.

## Applicability to Our Project
**Classification:** Directly applicable
## Why: implemented already; this paper defines the baseline behavior we must measure (success rate, time-to-first-incumbent) before adding objective awareness.

## Evidence → Engineering Decision
- *Finding:* FP gives first incumbents fast; our FP+rounding exist but are unmeasured against a reference → *PS requirement:* R5, R16 → *Component:* src/milp/heuristics.cpp → *Metric:* time-to-first-incumbent, primal integral, [[Relative Optimality Gap]]

## Related Papers
- [[Fischetti-2006-Feasibility-Pump-Heuristic]]
- [[Achterberg-2007-Improving-Feasibility-Pump]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Hillier-1969-Efficient-Heuristic-Procedures]]

## Uses
- [[Feasibility Pump]] [[Rounding Heuristic]] [[LP Relaxation]] [[Dual Simplex]]
