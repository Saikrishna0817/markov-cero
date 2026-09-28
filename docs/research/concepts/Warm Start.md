---
type: concept
tags: [concepts, milp]
status: stable
verified_on: 2026-09-25
---

# Warm Start

> Hand the node LP its parent's basis and most of the work is already done — this is where branch-and-bound actually wins.

## Definition
A warm start reuses the optimal basis (and preferably its factorization) of a closely related LP — a parent tree node, a re-solve after cut insertion, or a previous solve of the same model — as the starting point of the next solve, instead of crashing to a fresh basis. Because branching perturbs only one bound, the parent basis is usually dual-feasible but no longer primal-feasible, which makes the **dual simplex** the natural vehicle: it iterates from dual feasibility back to primal feasibility. A warm start must be validated (dimensions, model identity, tolerances) before it is trusted.

## Why It Matters Here
- R5 (branch-and-bound, node selection) and R20 (solve times on industrial instances) both depend on node LPs being cheap; re-crashing each node is the classic way to lose an order of magnitude.
- Observed state: `solve_node_relaxation` warm-starts from the parent basis via `BasisState` (src/milp/node_lp.cpp:45-77); the dual engine validates the warm basis against a 16-hex FNV-style fingerprint and dimension checks, with `allow_cold_fallback` default true (src/lp/dual/dual_simplex.cpp:123-145, 425-431).
- Inference: a cold start is delegated to the reference engine ("M3 oracle"), so warm-start quality directly determines whether MIP nodes run the fast path at all.

## Key Facts / Rules
- Dual simplex accepts a dual-feasible/primal-infeasible basis; bound changes are exactly what makes a basis primal-infeasible.
- Fingerprint/guard checks prevent handing a basis to a model it was not produced for.
- Across parallel workers there is no shared basis cache — each worker warm-starts only from its own parent (src/milp/parallel_tree_search.cpp).
- Crossover from an interior point is the *other* route to a basis (see [[Crossover]]); it does not exist here yet.

## Related
- [[Basis]]
- [[Dual Simplex]]
- [[Branch and Bound]]
- [[Crossover]]
- [[DualSimplexEngine]]

## Referenced By

- [[Research MOC|research/Research MOC]]
- [[Branch and Bound|research/algorithms/Branch and Bound]]
- [[Dual Simplex|research/algorithms/Dual Simplex]]
- [[Strong Branching|research/algorithms/Strong Branching]]
- [[Basis|research/concepts/Basis]]
- [[Crossover|research/concepts/Crossover]]
- [[No Crossover|research/limitations/No Crossover]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Bussieck-2026-mipfeas-Benchmark|research/papers/Bussieck-2026-mipfeas-Benchmark]]
- [[COIN-OR-n.d.-CBC-Solver-Documentation|research/papers/COIN-OR-n.d.-CBC-Solver-Documentation]]
- [[Canturk-2024-Scalable-Primal-Heuristics|research/papers/Canturk-2024-Scalable-Primal-Heuristics]]
- [[Danna-2004-Exploring-Relaxation-Induced|research/papers/Danna-2004-Exploring-Relaxation-Induced]]
- [[Driebeek-1966-Algorithm-Assignment-Problem|research/papers/Driebeek-1966-Algorithm-Assignment-Problem]]
- [[Fischetti-2006-Feasibility-Pump-Heuristic|research/papers/Fischetti-2006-Feasibility-Pump-Heuristic]]
- [[Fischetti-2015-Improving-Branch-Cut|research/papers/Fischetti-2015-Improving-Branch-Cut]]
- [[Fourer-1982-Solving-Linear-Programs|research/papers/Fourer-1982-Solving-Linear-Programs]]
- [[Fourer-n.d.-Hierarchical-Solution-Large|research/papers/Fourer-n.d.-Hierarchical-Solution-Large]]
- [[Held-2006-Lookahead-Branching-Mixed|research/papers/Held-2006-Lookahead-Branching-Mixed]]
- [[Maros-1993-Practical-Anti-Degeneracy|research/papers/Maros-1993-Practical-Anti-Degeneracy]]
- [[Maros-2003-Generalized-Dual-Phase|research/papers/Maros-2003-Generalized-Dual-Phase]]
- [[Neveu-2016-Node-Selection-Strategies|research/papers/Neveu-2016-Node-Selection-Strategies]]
- [[Schweizer-0000-Restart-Strategies-MIP|research/papers/Schweizer-0000-Restart-Strategies-MIP]]
- [[Unknown-2025-Apollo-MILP-Alternating|research/papers/Unknown-2025-Apollo-MILP-Alternating]]
- [[Ye-1998-Crossover-Interior-Point|research/papers/Ye-1998-Crossover-Interior-Point]]