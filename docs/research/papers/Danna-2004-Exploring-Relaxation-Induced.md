---
type: paper
title: "Exploring Relaxation Induced Neighborhoods to Improve MIP Solutions (RINS)"
authors: "Danna, Rothberg & Le Pape"
year: 2004
venue: "Math. Prog."
doi: "(unverified)"
domain: [heuristics]
priority: ★
status: deep
tags: [paper, heuristics]
---

# Exploring Relaxation Induced Neighborhoods to Improve MIP Solutions (RINS)

> RINS: fix every variable where the LP relaxation and the incumbent agree, then solve the much smaller sub-MIP around them.

## Metadata
| Field | Value |
|---|---|
| Authors | Danna, Rothberg & Le Pape |
| Year | 2004 |
| Venue | Mathematical Programming (list: "Math. Prog.") |
| DOI/URL | (unverified) |

## Problem Addressed
Local search needs a promising region; the LP relaxation alone often disagrees with the incumbent everywhere, so naive neighborhoods (e.g. local branching radius) are poorly centered.

## Core Contribution
- **Methodology:** Given incumbent x* and LP solution x̄, define the neighborhood by fixing all variables where x*_j = ⌊x̄_j⌋ or ⌈x̄_j⌉ (agreement set), then run a limited branch-and-bound inside that sub-MIP to find a better incumbent.
- **Assumptions:** An incumbent already exists; LP solution available at a node; sub-MIP solve is time/ node-limited.
- **Benchmarks/datasets:** MIPLIB-era instances; strong incumbent-improvement results reported.
- **Metrics:** Incumbent quality improvement, sub-MIP size, time per invocation.
- **Key results:** Neighborhood adapts to the instance (agreement set shrinks as LP and incumbent converge); one of the most effective generic primal heuristics and standard in SCIP/Gurobi/CPLEX.

## Engineering-Relevant Knowledge
**Algorithms:** RINS — relaxation-induced neighborhood search; limited sub-tree solve.
**Techniques:** Variable agreement detection; early termination of sub-MIP; interleaving with the main tree (invoked periodically, not every node).
**Implementation details:** Requires solving a sub-MIP — we can do this by re-entering `src/milp/milp_solver.cpp` with a fixed-variable mask and a node cap. RINS is **not** in `src/milp/heuristics.cpp` today (we have rounding + FP only).
**Equations/rules:** Neighborhood N = { x : x_j fixed to x*_j for all j with x*_j ∈ {⌊x̄_j⌋, ⌈x̄_j⌉} }; |N| = number of agreeing variables.
**Limitations/failure cases:** Useless before the first incumbent exists; sub-MIP solves compete for time with the main search — needs a budget.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Named in the audit gap list ("needs dive/RINS"); bigger implementation step than FP tuning but the standard path to good incumbents (R5, R20).

## Evidence → Engineering Decision
- *Finding:* heuristics limited to rounding + FP; no RINS/diving → *PS requirement:* R5, R20 → *Component:* src/milp/heuristics.cpp, src/milp/milp_solver.cpp → *Metric:* primal integral, time-to-best, [[Relative Optimality Gap]]

## Related Papers
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Fischetti-2003-Local-Branching]]
- [[Berthold-2007-RENS-Relaxation-Enforced]]
- [[Achterberg-2007-Improving-Feasibility-Pump]]

## Uses
- [[Branch and Bound]] [[LP Relaxation]] [[Warm Start]] [[Rounding Heuristic]]
