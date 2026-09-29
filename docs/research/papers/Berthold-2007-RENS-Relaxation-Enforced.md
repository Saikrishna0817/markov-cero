---
type: paper
title: "RENS — Relaxation Enforced Neighborhood Search"
authors: "Berthold"
year: 2007
venue: "ZIB"
doi: "(unverified)"
domain: [heuristics]
priority: ○
status: standard
tags: [paper, heuristics]
---

# RENS — Relaxation Enforced Neighborhood Search

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Fix every *fractional* variable of the LP solution to its floor/ceiling and solve the resulting sub-MIP — the mirror image of RINS.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold |
| Year | 2007 |
| Venue | ZIB Report (list: "ZIB") |
| DOI/URL | (unverified) |

## Problem Addressed
RINS (#113) needs an incumbent *and* an LP solution and exploits where they agree. Before a good incumbent exists, or when they disagree everywhere, a different neighborhood definition is needed.

## Core Contribution
- **Methodology:** Take the LP solution x̄; fix each fractional integer variable j to either ⌊x̄_j⌋ or ⌈x̄_j⌉ (branching on all fractional variables simultaneously = a full-depth sub-tree), then solve that sub-MIP with a limit.
- **Assumptions:** LP solution available; sub-MIP bounded by a node/time limit; incumbent optional (RENS also creates the first one).
- **Benchmarks/datasets:** MIPLIB-era instances; part of the SCIP LNS suite.
- **Metrics:** Incumbents found, sub-MIP size, time per call.
- **Key results:** Neighborhood is purely relaxation-derived, so it works before any incumbent exists and complements RINS/local branching.

## Engineering-Relevant Knowledge
**Algorithms:** RENS (relaxation-enforced neighborhood search); sub-MIP solve with all-fractional fixing.
**Techniques:** Simultaneous branching on the fractional set; depth/limit control; scheduled invocation from the main tree.
**Implementation details:** Requires a sub-MIP solve with fixed bounds — feasible with `src/milp/milp_solver.cpp` under a node budget; pairs naturally with our FP (FP first, RENS later). Not implemented.
**Equations/rules:** N = { x : x_j ∈ {⌊x̄_j⌋, ⌈x̄_j⌉} ∀ j fractional in x̄ }, all other variables fixed at LP values or left free (variant-dependent).
**Limitations/failure cases:** Sub-MIP can still be huge when many variables are fractional; costs LP solves for bounding.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Gives a first-incumbent-capable LNS tier beyond FP, addressing the audit's "needs dive/RINS" gap without new theory.

## Evidence → Engineering Decision
- *Finding:* no LNS-tier heuristic in our inventory → *PS requirement:* R5, R20 → *Component:* src/milp/heuristics.cpp → *Metric:* primal integral, time-to-first-incumbent

## Related Papers
- [[Danna-2004-Exploring-Relaxation-Induced]]
- [[Fischetti-2003-Local-Branching]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Fischetti-2005-Feasibility-Pump]]

## Uses
- [[LP Relaxation]] [[Diving]] [[Branch and Bound]] [[Rounding Heuristic]]
