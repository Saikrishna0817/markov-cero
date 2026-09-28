---
type: paper
title: "Primal Heuristics for Mixed Integer Programs (MS thesis)"
authors: "Berthold"
year: 2006
venue: "TU Berlin"
doi: "(unverified)"
domain: [heuristics]
priority: ○
status: standard
tags: [paper, heuristics]
---

# Primal Heuristics for Mixed Integer Programs (MS thesis)

> Extended treatment of the SCIP heuristic taxonomy: derivations, additional experiments and scheduling detail behind the 2007 ZIB report.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold |
| Year | 2006 |
| Venue | M.S. thesis, TU Berlin (list: "TU Berlin") |
| DOI/URL | (unverified) |

## Problem Addressed
The conference/ZIB writeup (#115) compresses derivations; the thesis asks for the full treatment of when and why each heuristic family works on general MIPs.

## Core Contribution
- **Methodology:** Full exposition of rounding, diving, objective diving, FP, RENS/RINS/local branching with detailed computational comparison and scheduling rules.
- **Assumptions:** Branch-and-cut with LP relaxations and incumbent handling.
- **Benchmarks/datasets:** MIP benchmark sets (same family as #115).
- **Metrics:** Primal integral, incumbents found, time share.
- **Key results:** Long-form version of #115 — the place to look for parameter defaults (frequencies, limits, tolerances).

## Engineering-Relevant Knowledge
**Algorithms:** Full primal-heuristic catalogue with pseudocode-level detail.
**Techniques:** Heuristic scheduling/limits; tolerance choices for rounding feasibility checks.
**Implementation details:** Use alongside #115 as the specification for extending `src/milp/heuristics.cpp`; parameter values (iteration caps, depths) come from here.
**Equations/rules:** Feasibility tolerances for accepting rounded solutions must match the LP tolerances of `src/lp/dual/dual_simplex.cpp` (else heuristics "succeed" with infeasible points).
**Limitations/failure cases:** Same as #115 (cost of LNS tiers); thesis-level detail may lag later solver practice.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Serves as the implementation reference (defaults and tolerances) for the heuristic expansion required by R5.

## Evidence → Engineering Decision
- *Finding:* heuristic acceptance tolerances must match LP tolerances → *PS requirement:* R9, R17 → *Component:* src/milp/heuristics.cpp, src/verify/primal_verifier.cpp → *Metric:* feasibility of incumbents ([[KKT Residual]])

## Related Papers
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2007-RENS-Relaxation-Enforced]]
- [[Berthold-2023-Feasibility-Jump]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics]]

## Uses
- [[Diving]] [[Feasibility Pump]] [[Rounding Heuristic]] [[KKT Residual]]
