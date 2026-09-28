---
type: paper
title: "Efficient Heuristic Procedures for Integer LP with an Interior"
authors: "Hillier"
year: 1969
venue: "Operations Research"
doi: "(unverified)"
domain: [heuristics]
priority: ○
status: standard
tags: [paper, heuristics]
---

# Efficient Heuristic Procedures for Integer LP with an Interior

> Ancestor of modern rounding heuristics: exploit an interior (bounded polytope) to derive near-feasible integer points by bounds-based rounding.

## Metadata
| Field | Value |
|---|---|
| Authors | Hillier |
| Year | 1969 |
| Venue | Operations Research (list: "Operations Research") |
| DOI/URL | (unverified) |

## Problem Addressed
Before branch-and-bound was practical, integer LPs needed heuristics that returned good integer points quickly; pure LP solutions are fractional and pure enumeration is slow.

## Core Contribution
- **Methodology:** Use the LP optimum plus variable bounds to construct integer candidate solutions (bounds-guided rounding and interior-based procedures) and evaluate them cheaply.
- **Assumptions:** Bounded LP ("with an interior"); integer variables with known bounds; feasibility not guaranteed.
- **Benchmarks/datasets:** Small integer programs of the era.
- **Metrics:** Quality of the integer point found; computation time vs. enumeration.
- **Key results:** Early demonstration that cheap rounding from the LP solution can be effective — the conceptual ancestor of LP rounding heuristics in every modern solver.

## Engineering-Relevant Knowledge
**Algorithms:** Bounds-based rounding; interior-based integer point construction.
**Techniques:** Round toward the feasible interior rather than blindly to nearest integer; evaluate before accepting.
**Implementation details:** Our rounding heuristic (`src/milp/heuristics.cpp`) is the direct descendant; the paper's caution is that rounding must respect *all* constraints, not just variable bounds — the check we delegate to `src/verify/primal_verifier.cpp`.
**Equations/rules:** Candidate x̂_j = clamp(⌊x̄_j⌉, l_j, u_j), then feasibility check on all rows before accepting.
**Limitations/failure cases:** Bounds-only reasoning fails on coupling constraints (strong LP relaxations with tight rows) — the reason FP/RINS were invented.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Historical grounding for the rounding heuristic we already ship; no new component, but it justifies the "round then verify" structure.

## Evidence → Engineering Decision
- *Finding:* rounding without constraint awareness yields infeasible "incumbents" → *PS requirement:* R9, R17 → *Component:* src/milp/heuristics.cpp, src/verify/primal_verifier.cpp → *Metric:* feasibility rate of heuristic outputs

## Related Papers
- [[Fischetti-2005-Feasibility-Pump]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2006-Primal-Heuristics-Mixed]]

## Uses
- [[Rounding Heuristic]] [[LP Relaxation]] [[Reduced Cost]] [[Feasibility Pump]]
