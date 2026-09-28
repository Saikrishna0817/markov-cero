---
type: metric
tags: [metrics, numerics]
status: stable
verified_on: 2026-09-25
---

# Primal Residual

> ‖Ax − b‖ (and bound violation) — how far the iterate is from actually satisfying the constraints.

## Definition
The primal residual measures constraint violation of the current point: for equality-constrained LP it is ‖Ax − b‖ (1- or ∞-norm, normalized by ‖b‖ for relative comparison); for inequality form it is the violation of l ≤ Ax ≤ u plus variable bound violations. In first-order methods it is one of three stopping signals (primal residual, dual residual, duality gap) and it behaves differently from the dual residual — a method can satisfy one while the other stagnates, which is why both are tracked. In ADMM the primal residual is r = Ax − z (the consensus gap between the two split variables) and the dual residual is s = ρAᵀ(z − z_prev).

## Why It Matters Here
- R9 (reliable convergence): "converged" must mean measured, and the primal residual is the direct measure of feasibility of the iterate.
- Observed state: PDLP's `optimal` status requires primal, dual and gap residuals all ≤ tolerance simultaneously (src/lp/first_order/pdlp.cpp:333-346); restarts score iterations using residual magnitudes (src/lp/first_order/pdlp.cpp:327-390).
- Observed state: ADMM's absolute/relative primal residual bounds are scaled by max(‖Ax‖, ‖z‖) so the tolerance is meaningful across scales (src/qp/admm_solver.cpp:200-210).

## Key Facts / Rules
- Primal residual: r_p = ‖Ax − b‖ (eq.) / violation of l ≤ Ax ≤ u (ineq.), normalized for relative tests.
- ADMM consensus form: r = Ax − z; dual residual s = ρAᵀ(z − z⁻).
- Stopping needs *both* primal and dual residuals plus gap — any one alone can be gamed by a degenerate iterate.
- Relative form: r_p ≤ ε_abs + ε_rel·max(‖b‖, ‖Ax‖) is the standard test.

## Related
- [[KKT Residual]]
- [[Primal-Dual Hybrid Gradient]]
- [[ADMM]]
- [[Duality Gap]]

## Referenced By

- [[16-testing-evaluation-strategy|audit/16-testing-evaluation-strategy]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Evaluation MOC|research/Evaluation MOC]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[KKT Residual|research/metrics/KKT Residual]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm|research/papers/Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]