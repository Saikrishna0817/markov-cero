---
type: concept
tags: [techniques, gpu]
status: stable
verified_on: 2026-09-25
---

# Diagonal Preconditioning

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Rescale variables and constraints so every proximal/step-size parameter is O(1) — the difference between a method that converges and one that crawls.

## Definition
Diagonal preconditioning replaces the isotropic step sizes of a first-order method with per-coordinate steps inversely proportional to local matrix norms: τ_j ∝ 1/‖A_·j‖ and σ_i ∝ 1/‖A_i·‖ (typically 1-norms or 2-norms), often combined with a scalar η controlling overall step size and a primal-dual weight ω balancing the two sides. The effect is to make the effective operator A well-scaled so the convergence condition στ‖A‖² < 1 is satisfied with the largest useful steps, and so progress is made uniformly across rows/columns of very different magnitudes. In splitting methods (ADMM) the analogue is the per-constraint penalty ρ_i, chosen from row norms or adapted during iterations.

## Why It Matters Here
- R8/R9: PDHG on unscaled industrial data (R11 coefficient ranges) is unusable without this; it is a preconditioner, not a model change — no unscale needed for the iterate itself, but the model scaling still matters for tolerances.
- Observed state: CPU PDLP sets `tau = (eta/omega)/col_norm`, `sigma = (eta*omega)/row_norm` from row/column 1-norms and initializes ω = sqrt(‖c‖∞/‖b‖∞) clamped to [0.01, 100] (src/lp/first_order/pdlp.cpp:232-244); device state carries the same tau/sigma preconditioners (gpu/include/markov_cero/gpu/pdhg_step.hpp:20-51).
- Observed state: ADMM adapts per-constraint ρ into [1e-6, 1e6] every 25 iterations with numeric refactorization (src/qp/admm_solver.cpp:288-303).

## Key Facts / Rules
- Steps: τ_j = η/(ω·‖A_·j‖), σ_i = ηω/‖A_i·‖; admissibility requires στ‖A‖² ≤ 1 (Chambolle–Pock condition).
- Primal weight ω balances primal vs dual residual magnitudes; updated on restart as ω ← ω·(√(r_p/r_d))^0.5.
- Diagonal preconditioning ≠ model scaling: it changes algorithm parameters, not the stored matrix — both are used together here.
- For ADMM the analogous knob is ρ (penalty), which controls the same tradeoff between primal and dual convergence.

## Related
- [[Primal-Dual Hybrid Gradient]]
- [[ADMM]]
- [[Scaling]]
- [[Ruiz Scaling]]
- PDLP-Engine

## Referenced By

- Architecture MOC
- [[Primal-Dual Hybrid Gradient|research/algorithms/Primal-Dual Hybrid Gradient]]
- [[Adaptive Restart|research/techniques/Adaptive Restart]]