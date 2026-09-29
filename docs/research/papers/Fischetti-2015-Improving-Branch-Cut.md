---
type: paper
title: "Improving Branch-and-Cut Performance by Random Sampling"
authors: "Fischetti, Lodi, Monaci, Salvagnin & Tramontani"
year: 2015
venue: "Mathematical Programming Computation"
doi: "(unverified)"
domain: [parallel]
priority: ★
status: deep
tags: [paper, parallel]
---

# Improving Branch-and-Cut Performance by Random Sampling

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Randomly samples alternative optima of the LP relaxation to harvest integer solutions fast — a cheap, embarrassingly parallel primal-heuristic boost for branch-and-cut.

## Metadata
| Field | Value |
|---|---|
| Authors | Fischetti, Lodi, Monaci, Salvagnin & Tramontani |
| Year | 2015 |
| Venue | Mathematical Programming Computation |
| DOI/URL | http://www.dei.unipd.it/~fisch/papers/improving_branch_and_cut_performance_by_random_sampling.pdf |

## Problem Addressed
A branch-and-cut solver spends most of its time in the tree; a strong incumbent prunes large parts of it, but incumbents are hard to find. The paper asks how to convert the LP relaxation's (possibly large) optimal face into many integer-feasible candidates without extra LP solves, and how to do it in parallel.

## Core Contribution
- **Methodology:** Draw random convex combinations / alternative optima of the LP relaxation (solved once or a few times), map samples to integer candidates by rounding or by moving along the optimal face, then intensify with large-neighborhood steps (RINS-style) when a promising candidate appears.
- **Assumptions:** The LP relaxation has multiple optima or a non-degenerate optimal face to sample; candidate repair is cheaper than a full node LP.
- **Benchmarks/datasets:** MIPLIB-style MIP instances (study includes MIPLIB 2003-era sets; exact per-instance results not re-verified here).
- **Metrics:** Number of feasible solutions found, primal bound over time / primal integral, impact on total branch-and-cut time.
- **Key results:** Sampling plus neighborhood intensification improves the primal side of branch-and-cut substantially on instances where the LP has many optima; the approach was adopted commercially (CPLEX 12.5.1, per source list).

## Engineering-Relevant Knowledge
**Algorithms:** Alternative-optima sampling, rounding-based candidate construction, interaction with RINS/local branching.
**Techniques:** Reuse one LP basis/optimal face rather than solving many LPs; embarrassingly parallel sampling across threads — a better parallel workload than tree splitting when the tree is small.
**Implementation details:** Our heuristics live in `src/milp/heuristics.cpp` and node LPs in `src/milp/node_lp.cpp`; sampling requires access to alternate optima from the LP engine (dual degeneracy / multiple optima), which our simplex does not currently expose.
**Equations/rules:** Candidate x = Σ λ_i x^(i) over sampled optimal vertices, then project/round to integer and test feasibility.
**Limitations/failure cases:** Little to sample when the LP optimum is unique and far from integrality; gains depend on degenerate/multiple optima — the same instances where our solver is weakest.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It gives us a parallel axis (sample threads) that works on small trees where tree-level parallelism provably cannot help, and it targets primal bounds — our cuts currently show 0.0% node reduction, so incumbents are our remaining pruning lever.

## Evidence → Engineering Decision
- *Finding:* Cut node reduction is 0.0% on stein9/flugpl (evidence/benchmarks/phase4.json), so pruning comes only from incumbents → *PS requirement:* R5 → *Component:* src/milp/heuristics.cpp → *Metric:* [[Relative Optimality Gap]] at root and at time limit
- *Finding:* 4-thread tree parallelism yields 0.56× while sampling scales per thread → *PS requirement:* R7 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* [[Parallel Speedup]]

## Related Papers
- [[Eckstein-1994-Control-Strategies-Parallel]]
- [[Lodi-2013-Performance-Variability-Mixed]]
- [[Bussieck-2026-mipfeas-Benchmark]]
- [[Neumaier-2004-Safe-Bounds-Linear]]

## Uses
- [[Warm Start]]
- [[Branch and Cut]]
- [[Degeneracy]]
