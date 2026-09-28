---
type: concept
tags: [algorithms, lp]
status: stable
verified_on: 2026-09-25
---

# Dual Simplex

> Keep dual feasibility, restore primal feasibility — the re-optimization workhorse that makes branch-and-bound affordable.

## Definition
The dual simplex starts from a basis that is dual-feasible but primal-infeasible (typically after a bound change or cut insertion) and pivots to restore primal feasibility while preserving dual feasibility. It is the algebraic mirror of the primal method: the ratio test is applied to the tableau row of the entering dual candidate, and the leaving-variable choice drives primal infeasibility down. Because a child node's basis is inherited from its parent and only one bound changed, dual feasibility is preserved for free — which is why modern MIP solvers solve nearly every node with a warm-started dual simplex.

## Why It Matters Here
- R5 (branch-and-bound) and R20 (industrial solve times) both ride on node LP cost; the dual method is the intended fast path.
- Observed state: `solve(model, options, warm_start)` at src/lp/dual/dual_simplex.cpp:363 with `harris_ratio` default true, `tableau_norm` pricing explicitly documented as *not* conventional dual steepest-edge, a pivot-ratio condition trigger at 1e-14, and Farkas certificate checks on dual infeasibility (src/lp/dual/dual_simplex.cpp:271-411).
- Observed state: no warm start ⇒ cold solve is delegated to the reference engine (src/lp/dual/dual_simplex.cpp:388-390), so the dual path is warm-start-only in practice.

## Key Facts / Rules
- Invariant: dual feasibility maintained; primal infeasibility (negative basic values) is reduced each iteration.
- Same caps as the reference engine (1024×8192, 1e6 iterations) — Inference: scale ceiling is shared, not method-specific.
- Harris two-pass ratio test is the default acceptance rule for the leaving-row choice.
- Dual infeasibility of the phase ⇒ Farkas certificate of primal unboundedness/infeasibility, verified post-hoc.

## Related
- [[Warm Start]]
- [[Revised Simplex]]
- [[Reduced Cost]]
- [[Steepest Edge]]
- [[DualSimplexEngine]]
- [[Koberstein-2005-Dual-Simplex-Method]]

## Referenced By

- [[DualSimplexEngine|codebase/components/DualSimplexEngine]]
- [[IndependentVerifiers|codebase/components/IndependentVerifiers]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Research-Code Traceability MOC|research/Research-Code Traceability MOC]]
- [[Revised Simplex|research/algorithms/Revised Simplex]]
- [[Steepest Edge|research/algorithms/Steepest Edge]]
- [[Warm Start|research/concepts/Warm Start]]
- [[ED-002-keep-simplex-core-add-first-order-not-replace|research/engineering-decisions/ED-002-keep-simplex-core-add-first-order-not-replace]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Benichou-1997-Linear-Programming-Implementations|research/papers/Benichou-1997-Linear-Programming-Implementations]]
- [[Berthold-2013-Cloud-Branching|research/papers/Berthold-2013-Cloud-Branching]]
- [[Berthold-2023-Feasibility-Jump|research/papers/Berthold-2023-Feasibility-Jump]]
- [[Charnes-1954-Optimality-Multi-Valuedness|research/papers/Charnes-1954-Optimality-Multi-Valuedness]]
- [[Chinneck-1987-Primal-Dual-Methods|research/papers/Chinneck-1987-Primal-Dual-Methods]]
- [[DeFarias-2019-Positive-Edge-Pricing|research/papers/DeFarias-2019-Positive-Edge-Pricing]]
- [[Fischetti-2005-Feasibility-Pump|research/papers/Fischetti-2005-Feasibility-Pump]]
- [[Fourer-1982-Solving-Linear-Programs|research/papers/Fourer-1982-Solving-Linear-Programs]]
- [[Gill-1974-Methods-Modifying-Matrix|research/papers/Gill-1974-Methods-Modifying-Matrix]]
- [[Gill-1989-Practical-Anti-Cycling|research/papers/Gill-1989-Practical-Anti-Cycling]]
- [[Gomory-1958-Outline-Algorithm-Integer|research/papers/Gomory-1958-Outline-Algorithm-Integer]]
- [[Gomory-1963-All-Integer-Programming|research/papers/Gomory-1963-All-Integer-Programming]]
- [[Gondzio-1994-Another-Simplex-Type|research/papers/Gondzio-1994-Another-Simplex-Type]]
- [[Huangfu-2018-Parallelizing-Dual-Revised|research/papers/Huangfu-2018-Parallelizing-Dual-Revised]]
- [[Koberstein-2005-Dual-Simplex-Method|research/papers/Koberstein-2005-Dual-Simplex-Method]]
- [[Maros-0000-New-Degeneracy-Method|research/papers/Maros-0000-New-Degeneracy-Method]]
- [[Maros-1993-Practical-Anti-Degeneracy|research/papers/Maros-1993-Practical-Anti-Degeneracy]]
- [[Maros-2003-Generalized-Dual-Phase|research/papers/Maros-2003-Generalized-Dual-Phase]]
- [[Steinrucken-2019-Exact-Algorithms-Linear|research/papers/Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Harris Ratio Test|research/techniques/Harris Ratio Test]]