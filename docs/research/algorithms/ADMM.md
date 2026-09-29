---
type: concept
tags: [algorithms, qp]
status: stable
verified_on: 2026-09-25
---

# ADMM

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Alternate a KKT solve, a projection and a dual ascent — the splitting method behind this solver's QP/MIQP engine.

## Definition
The Alternating Direction Method of Multipliers splits a constrained problem into two easily handled pieces (here: a quadratic objective piece and a constraint/box piece) coupled by an augmented Lagrangian with penalty ρ. One ADMM iteration solves an augmented KKT system for the primal update, over-relaxes with parameter α, projects onto the simple feasible set (a box), then performs a dual ascent step y ← y + ρ(ẑ − z). For convex QP with l ≤ Ax ≤ u the KKT matrix is quasi-definite, so a pivoting-free sparse LDLᵀ factorization exists and is reused while ρ is fixed; adaptive ρ changes require numeric refactorization. Convergence is to a KKT point with ergodic (residual) rates — first-order, like PDHG.

## Why It Matters Here
- R2 (QP in initial scope) and R3 (extensible architecture) are satisfied today largely *through* this engine: OSQP-style ADMM over a quasi-definite KKT system.
- Observed state: defaults `rho_init 0.1`, `alpha 1.6`, `sigma 1e-6`, tol 1e-4, `max_iterations 4000`, adaptive ρ every 25 iterations with `update_numeric` refactorization; infeasibility certificates follow Banjac et al. 2019 and are checked every 10 iterations (src/qp/admm_solver.cpp:84-303).
- Observed state: it also serves MIQP node relaxations (src/milp/node_lp.cpp:15-38); non-convex Q is rejected rather than solved (src/qp/model.cpp:142-206).

## Key Facts / Rules
- Iteration: KKT solve → over-relaxation x̂ = αx̃ + (1−α)x → project z onto box → dual update y += ρ(ẑ − z).
- Convergence uses absolute + relative primal and dual residual bounds scaled by ‖Ax‖, ‖Aᵀy‖ magnitudes (src/qp/admm_solver.cpp:200-210).
- Quasi-definite KKT (σ > 0, ρ > 0) ⇒ non-singular LDLᵀ without pivot search.
- Adaptive ρ trades refactorization cost against residual balance.

## Related
- [[KKT Conditions]]
- [[Sparse LDL Factorization]]
- [[Primal-Dual Hybrid Gradient]]
- QP-ADMM-Engine
- [[Vanderbei-1995-Symmetric-Indefinite-Systems]]

## Referenced By

- 21-traceability
- LDL-Factorization
- QP-ADMM-Engine
- Algorithms MOC
- Architecture MOC
- Research MOC
- [[Sparse LDL Factorization|research/algorithms/Sparse LDL Factorization]]
- [[KKT Conditions|research/concepts/KKT Conditions]]
- [[QPLIB|research/datasets/QPLIB]]
- [[First-Order Accuracy Ceiling|research/limitations/First-Order Accuracy Ceiling]]
- cross-paper-synthesis
- [[Primal Residual|research/metrics/Primal Residual]]
- [[Goldfarb-1983-Numerically-Stable-Dual|research/papers/Goldfarb-1983-Numerically-Stable-Dual]]
- [[Unknown-2025-Overview-GPU-Based-First|research/papers/Unknown-2025-Overview-GPU-Based-First]]
- [[Diagonal Preconditioning|research/techniques/Diagonal Preconditioning]]