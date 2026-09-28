---
type: concept
tags: [algorithms, gpu]
status: stable
verified_on: 2026-09-25
---

# Primal-Dual Hybrid Gradient

> A first-order saddle-point method whose only kernels are SpMV and axpy — the reason a GPU can compete on large LPs at all.

## Definition
PDHG (Chambolle–Pock 2011) solves a saddle-point problem min_x max_y L(x, y) by alternating a primal proximal step, a dual proximal step and an extrapolation (x̄ = 2x − x_prev), with step sizes σ, τ constrained by στ‖A‖² < 1. For LP it is applied to the primal-dual form of min cᵀx s.t. Ax = b, x ≥ 0, where the prox steps reduce to projections onto boxes and the only matrix work is A x and Aᵀ y. Restarted/adaptive variants (PDLP; cuPDLP.jl) add restart on residual score, adaptive step sizes and primal-weight balancing to make the O(1/k) ergodic rate usable at high accuracy. The payoff is perfect GPU mapping; the cost is iteration count and a first-order accuracy ceiling.

## Why It Matters Here
- R8 allows GPU "where it provides measurable benefits" and R4/R2 cover LP; this is the engine positioned to claim that benefit.
- Observed state: CPU engine with τ/σ from row/column norms, adaptive Lipschitz retargeting every 10 iterations, restart every 40 by default, primal weight updates on restart (src/lp/first_order/pdlp.cpp:232-390); device path keeps iterates resident and times H2D/kernel/D2H separately (gpu/src/pdhg_step.cpp:399-443).
- Observed state: `evidence/benchmarks/crossover_study.csv` shows end-to-end GPU speedup < 1 in 13/13 rows — Inference: the algorithm maps to the GPU well, but the benefit claim is currently unproven (see [[GPU Benefit Unproven]]).

## Key Facts / Rules
- Convergence test: primal residual, dual residual *and* gap must all be ≤ tolerance before `optimal` is reported (src/lp/first_order/pdlp.cpp:333-346).
- Step sizes scale as τ ∝ 1/column-norm, σ ∝ 1/row-norm (diagonal preconditioning), then scaled by η/ω.
- Restarts reset the O(1/k) bound using the current point; adaptive restart fires on residual-score ratio ≈ 0.368.
- No basis is produced, so no warm start into MIP nodes — see [[First-Order Accuracy Ceiling]].

## Related
- [[Interior-Point Method]]
- [[Diagonal Preconditioning]]
- [[GPU CSR SpMV]]
- [[Ruiz Scaling]]
- [[PDLP-Engine]]
- [[Lu-2025-cuPDLP-GPU-Implementation]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[GPU-PDHG-Engine|codebase/components/GPU-PDHG-Engine]]
- [[PDLP-Engine|codebase/components/PDLP-Engine]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[ADMM|research/algorithms/ADMM]]
- [[Interior-Point Method|research/algorithms/Interior-Point Method]]
- [[Scaling|research/concepts/Scaling]]
- [[ED-002-keep-simplex-core-add-first-order-not-replace|research/engineering-decisions/ED-002-keep-simplex-core-add-first-order-not-replace]]
- [[First-Order Accuracy Ceiling|research/limitations/First-Order Accuracy Ceiling]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Primal Residual|research/metrics/Primal Residual]]
- [[Lu-2025-cuPDLP-GPU-Implementation|research/papers/Lu-2025-cuPDLP-GPU-Implementation]]
- [[Unknown-2025-Overview-GPU-Based-First|research/papers/Unknown-2025-Overview-GPU-Based-First]]
- [[GPU Benefit Unproven|research/research-gaps/GPU Benefit Unproven]]
- [[Adaptive Restart|research/techniques/Adaptive Restart]]
- [[Diagonal Preconditioning|research/techniques/Diagonal Preconditioning]]
- [[GPU CSR SpMV|research/techniques/GPU CSR SpMV]]
- [[Ruiz Scaling|research/techniques/Ruiz Scaling]]