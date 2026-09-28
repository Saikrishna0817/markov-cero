---
type: paper
title: "Feasibility Jump"
authors: "Berthold"
year: 2023
venue: "IJOC"
doi: "(unverified)"
domain: [heuristics]
priority: ○
status: standard
tags: [paper, heuristics]
---

# Feasibility Jump

> A fast local-repair heuristic: greedily flip/perturb variables to remove infeasibilities, no LP solves required.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold |
| Year | 2023 |
| Venue | INFORMS Journal on Computing (list: "IJOC") |
| DOI/URL | (unverified) |

## Problem Addressed
LP-based heuristics (FP, RINS) cost many LP solves; on small/medium MIPs the dominant need is a cheap, LP-free way to repair a rounded point into feasibility.

## Core Contribution
- **Methodology:** Start from a (rounded) point; repeatedly evaluate single-variable moves by how much they reduce total constraint violation, apply the best move, with randomization and a walk/tabu mechanism to escape local minima; stop on feasibility or iteration cap.
- **Assumptions:** Cheap incremental evaluation of constraint violation; bounded iteration count.
- **Benchmarks/datasets:** MIPLIB; recent studies show strong time-to-first-solution (also re-implemented in HiGHS, see #123).
- **Metrics:** Success rate, time per call (microseconds–milliseconds), primal integral contribution.
- **Key results:** Often finds feasible points faster than LP-based pumps on instances where LP solves dominate; ideal as an always-on fallback.

## Engineering-Relevant Knowledge
**Algorithms:** Feasibility Jump — greedy violation-reduction local search with random restarts.
**Techniques:** Incremental violation bookkeeping (only affected constraints re-evaluated after a flip); tabu/perturbation; variable move ordering.
**Implementation details:** Complements `src/milp/heuristics.cpp`: no LP dependency, so it can run even when [[Dual Simplex]] is struggling — valuable for the robustness story (R13). Constraint-violation evaluation must share tolerances with `src/verify/primal_verifier.cpp`.
**Equations/rules:** Move score = Δ(total violation) over constraints containing the variable; accept best negative score; accept worse moves with probability p for diversification.
**Limitations/failure cases:** Greedy myopia on strongly coupled rows; needs good incremental data structures or it is too slow per call.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Cheap, LP-free and modern — a strong addition to a thin heuristic set, and it demonstrates robustness (finding incumbents when LP is ill-conditioned).

## Evidence → Engineering Decision
- *Finding:* heuristics all depend on LP solves → *PS requirement:* R5, R13 → *Component:* src/milp/heuristics.cpp → *Metric:* time-to-first-incumbent on degenerate instances

## Related Papers
- [[Spoorendonk-2026-Presolve-Heuristics-HiGHS]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2006-Primal-Heuristics-Mixed]]
- [[Fischetti-2005-Feasibility-Pump]]

## Uses
- [[Feasibility Pump]] [[Rounding Heuristic]] [[Numerical Stability]] [[Degeneracy]]
