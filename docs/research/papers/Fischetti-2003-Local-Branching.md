---
type: paper
title: "Local Branching"
authors: "Fischetti & Lodi"
year: 2003
venue: "Math. Prog."
doi: "(unverified)"
domain: [heuristics]
priority: ★
status: deep
tags: [paper, heuristics]
---

# Local Branching

> Encodes a Hamming-distance ball around the incumbent as extra cuts and solves the restricted sub-MIP — local search as integer programming.

## Metadata
| Field | Value |
|---|---|
| Authors | Fischetti & Lodi |
| Year | 2003 |
| Venue | Mathematical Programming (list: "Math. Prog.") |
| DOI/URL | (unverified) |

## Problem Addressed
Given a good incumbent, how do you systematically search its neighborhood without running an unconstrained global search? A neighborhood must be defined without knowing the instance's structure.

## Core Contribution
- **Methodology:** Add a locality constraint limiting Hamming distance to the incumbent (Σ_{j: x*_j=0} x_j + Σ_{j: x*_j=1} (1 − x_j) ≤ δ for binaries) and branch inside that restricted region; enlarge δ and re-solve to extend the search.
- **Assumptions:** An incumbent exists; binary variables (generalizations exist); δ chosen small enough that the sub-MIP stays tractable.
- **Benchmarks/datasets:** TSP and MIPLIB-style instances of the era.
- **Metrics:** Incumbent improvement per unit time; sub-MIP node count vs. δ.
- **Key results:** Turned local search into a solvable sub-problem; one of the first "large neighborhood" MIP heuristics, and a natural fit for any solver that already has a branch-and-cut.

## Engineering-Relevant Knowledge
**Algorithms:** Local branching (distance-constrained sub-MIP); δ-expansion schedule.
**Techniques:** Hamming-distance cut; reuse of the main solver with a temporary row; time budget for the sub-search.
**Implementation details:** One extra row in `src/milp/milp_solver.cpp` (or a separate sub-solve in `src/milp/heuristics.cpp`); needs an incumbent first, so it runs after FP/rounding. Not implemented today.
**Equations/rules:** locality cut: Σ_{j∈B_0} x_j + Σ_{j∈B_1} (1 − x_j) ≤ δ, where B_0/B_1 are the incumbent's 0/1 support.
**Limitations/failure cases:** No incumbent ⇒ no neighborhood; δ too small ⇒ no improvement possible, δ too large ⇒ sub-MIP as hard as the original.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Cheap to implement given an existing B&B (one cut + re-solve) and a standard competitor to RINS for incumbent improvement (R5, R20).

## Evidence → Engineering Decision
- *Finding:* only FP + rounding provide incumbents → *PS requirement:* R5, R20 → *Component:* src/milp/heuristics.cpp → *Metric:* primal integral, [[Relative Optimality Gap]]

## Related Papers
- [[Danna-2004-Exploring-Relaxation-Induced]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2007-RENS-Relaxation-Enforced]]
- [[Fischetti-2005-Feasibility-Pump]]

## Uses
- [[Branch and Cut]] [[Rounding Heuristic]] [[LP Relaxation]] [[Diving]]
