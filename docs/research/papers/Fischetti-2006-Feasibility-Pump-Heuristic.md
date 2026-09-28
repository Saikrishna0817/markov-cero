---
type: paper
title: "A Feasibility Pump Heuristic for General MIPs"
authors: "Fischetti, Bertacco & Lodi"
year: 2006
venue: "Discrete Optim."
doi: "(unverified)"
domain: [heuristics]
priority: ★
status: deep
tags: [paper, heuristics]
---

# A Feasibility Pump Heuristic for General MIPs

> Extends the Feasibility Pump from 0/1 to general-integer and continuous variables via constrained/soft projections.

## Metadata
| Field | Value |
|---|---|
| Authors | Fischetti, Bertacco & Lodi |
| Year | 2006 |
| Venue | Discrete Optimization (list: "Discrete Optim.") |
| DOI/URL | (unverified; PDF link in reference list) |

## Problem Addressed
The original FP (#110) rounds binary variables only; real models (lot-sizing quantities, unit counts, refinery transfer volumes) contain unbounded general integers where "nearest integer" plus projection does not apply directly.

## Core Contribution
- **Methodology:** Handle non-binary integers by keeping them continuous but penalizing distance from their rounded value, and by a lexicographic/soft objective; add infeasibility-driven restarts so the pump does not stall on mixed rows.
- **Assumptions:** LP relaxation solvable at each step; bounds on integer variables for rounding (or handling for unbounded integers); objective ignored (feasibility only).
- **Benchmarks/datasets:** General MIP instances including MIPLIB (paper's framing).
- **Metrics:** Feasible-solution success rate, time to first incumbent.
- **Key results:** Makes FP usable on general MIPs — the version a real solver needs, since our test sets are not purely binary.

## Engineering-Relevant Knowledge
**Algorithms:** [[Feasibility Pump]] for general MIP; soft-rounding of continuous/general-integer columns.
**Techniques:** Penalty terms for rounding violation; lexicographic optimization (feasibility first, then objective); restart with random perturbation.
**Implementation details:** `src/milp/heuristics.cpp` must handle non-binary integrality — this paper specifies how (penalty rather than hard rounding). Worth checking whether our FP restricts to binaries today.
**Equations/rules:** min Σ_{j∈I} d_j(x_j) where d_j = 0 if x_j integer-feasible, else distance to nearest integer; combined with LP feasibility constraints.
**Limitations/failure cases:** Still objective-blind; can oscillate between infeasible integer points; cost is multiple LP solves per iteration.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Our instances (MILP scope R2, MIPLIB R15) include general integers, so the binary-only FP is insufficient; this is the spec for the general case.

## Evidence → Engineering Decision
- *Finding:* FP implemented in `src/milp/heuristics.cpp` but general-integer handling unverified → *PS requirement:* R2, R5, R15 → *Component:* src/milp/heuristics.cpp → *Metric:* incumbent success rate, time-to-first-solution

## Related Papers
- [[Fischetti-2005-Feasibility-Pump]]
- [[Achterberg-2007-Improving-Feasibility-Pump]]
- [[Berthold-2006-Primal-Heuristics-Mixed]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics]]

## Uses
- [[Feasibility Pump]] [[Rounding Heuristic]] [[LP Relaxation]] [[Warm Start]]
