---
type: concept
tags: [algorithms, lp]
status: stable
verified_on: 2026-09-25
---

# Interior-Point Method

> Barrier/path-following methods that trade a basis for polynomial complexity — the second LP method R4 names.

> **Implemented (2026-09-26).** `src/lp/interior/ipm.cpp` is a Mehrotra predictor-corrector
> IPM on the canonical standard form, dispatched by `--engine ipm`, with crossover to a
> certified vertex basis ([[Crossover]], [[No Crossover]]). The normal-equation system is
> factorized with a diagonal-perturbation fallback and the engine falls back to the reference
> primal simplex, with honest telemetry, when it cannot certify a solution.

## Definition
Interior-point methods solve LP (and convex QP) by following the central path x_j s_j = μ (complementarity parameterized by barrier μ), starting from a strictly interior point and reducing μ geometrically. Primal-dual variants solve a Newton step on the perturbed KKT system each iteration; Mehrotra's predictor-corrector (1992) estimates the affine step and corrects, and is the basis of essentially all production IPMs. Each iteration factorizes a (usually symmetric, often augmented) KKT matrix — sparse LDLᵀ or normal equations — so the linear algebra is different from simplex but the *data structures* (sparse CSC, ordering, refinement) are shared. Iteration counts are modest and insensitive to degeneracy, but the solution is interior: no basis, no vertex.

## Why It Matters Here
- R4 requires interior-point alongside revised simplex; PS-GAP-06 records that **no IPM exists** in the repo (docs/audit/00-ground-truth.md).
- Observed state: PDLP/PDHG is a *first-order* method, not a barrier method — no μ-path, no predictor-corrector, no KKT factorization inside the LP loop; QP uses ADMM instead.
- Inference: R4 is now met end-to-end: the IPM ships with [[Crossover]], so its interior
  optimum is converted into a basis that the MILP node path can consume.

## Key Facts / Rules
- Central path condition: x_j s_j = μ with primal/dual feasibility; Newton step solves the augmented KKT system.
- Mehrotra predictor-corrector: one affine prediction + one correction per iteration is the practical standard.
- Conditioning: symmetric indefinite KKT systems (Vanderbei 1995) vs normal equations — a conditioning/robustness tradeoff.
- Degeneracy does not stall IPM iterations, but it makes crossover and accuracy harder afterwards.

## Related
- [[Crossover]]
- [[KKT Conditions]]
- [[Primal-Dual Hybrid Gradient]]
- [[No Interior-Point Engine]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Wright-1997-Primal-Dual-IPM]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[LDL-Factorization|codebase/components/LDL-Factorization]]
- [[QP-ADMM-Engine|codebase/components/QP-ADMM-Engine]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Primal-Dual Hybrid Gradient|research/algorithms/Primal-Dual Hybrid Gradient]]
- [[Multi-Engine Solver Architecture|research/architectures/Multi-Engine Solver Architecture]]
- [[Crossover|research/concepts/Crossover]]
- [[QPLIB|research/datasets/QPLIB]]
- [[No Crossover|research/limitations/No Crossover]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Amestoy-2000-Parallel-Sparse-Linear|research/papers/Amestoy-2000-Parallel-Sparse-Linear]]
- [[Carpentier-1962-Origin-Economic-Dispatch|research/papers/Carpentier-1962-Origin-Economic-Dispatch]]
- [[Gondzio-1996-Multiple-Centrality-Corrections|research/papers/Gondzio-1996-Multiple-Centrality-Corrections]]
- [[Karmarkar-1984-New-Polynomial-Time-Algorithm|research/papers/Karmarkar-1984-New-Polynomial-Time-Algorithm]]
- [[Khachiyan-1979-Polynomial-Algorithm-Linear|research/papers/Khachiyan-1979-Polynomial-Algorithm-Linear]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm|research/papers/Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Kronqvist-2025-50-Years-Mixed-Integer|research/papers/Kronqvist-2025-50-Years-Mixed-Integer]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector|research/papers/Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Lustig-1994-Interior-Point-Methods|research/papers/Lustig-1994-Interior-Point-Methods]]
- [[Martinson-1999-Interior-Point-Dantzig-Wolfe|research/papers/Martinson-1999-Interior-Point-Dantzig-Wolfe]]
- [[McShane-1989-Implementation-Primal-Dual-Interior|research/papers/McShane-1989-Implementation-Primal-Dual-Interior]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior|research/papers/Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Tits-1994-Simple-Quadratically-Convergent|research/papers/Tits-1994-Simple-Quadratically-Convergent]]
- [[Unknown-n.d.-Accelerating-Optimization-Solvers|research/papers/Unknown-n.d.-Accelerating-Optimization-Solvers]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods|research/papers/Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Wright-2004-Interior-Point-Revolution|research/papers/Wright-2004-Interior-Point-Revolution]]
- [[Ye-1991-Interior-Point-Algorithm|research/papers/Ye-1991-Interior-Point-Algorithm]]
- [[No Interior-Point Engine|research/research-gaps/No Interior-Point Engine]]