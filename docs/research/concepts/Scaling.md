---
type: concept
tags: [concepts, numerics]
status: stable
verified_on: 2026-09-25
---

# Scaling

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Put rows and columns on comparable footing before anything numerical runs — the cheapest robustness win available.

## Definition
Scaling multiplies variables and constraints by positive diagonal matrices, Ã = D_R A D_C, so that row and column magnitudes are comparable, then undoes the transformation exactly on the returned primal, dual, rays and certificates. Equilibration-style methods iterate the rescaling until norms balance (Ruiz–Torres), while analytical methods derive scales from maxima/geometric means. Scaling does not change the mathematical problem — but it changes conditioning κ(A), the meaning of every absolute tolerance, and the behavior of ratio tests and stopping rules. A solver that scales must unscale *both* primal and dual outputs or its certificates become wrong.

## Why It Matters Here
- R9/R13: ill-conditioned industrial data (refinery coefficients spanning orders of magnitude, R11/R12) is exactly where unscaled tolerances fail.
- Observed state: `RuizScaling` applies D_R A D_C with `max_iterations 10`, `tolerance 1e-3`, scales clamped to [1e-4, 1e4], and `unscale_solution` rescales primal, dual, rays and Farkas certificates (src/scale/ruiz_scaling.cpp:82-145).
- Inference (observed gap): scaling runs only on the `primal`/`pdlp` CLI branches — MILP, QP and parallel branches never call `equilibrate` (no call site in src/milp/*), so tolerance semantics differ per engine.

## Key Facts / Rules
- Ruiz iteration: multiply the current diagonal by δ = 1/√(row or column 2-norm), repeat until max norm error < tolerance.
- Unscale exactly: primal by D_C, dual by D_R⁻¹ (as implemented in `unscale_solution`); approximate unscaling invalidates verification.
- Absolute tolerances are scale-dependent — Harris bands and feasibility tolerances are meaningless on wildly skewed data.
- Scaling is preprocessing, not a cure: κ can be reduced orders of magnitude but not to 1.

## Related
- [[Ill-Conditioning]]
- [[Numerical Stability]]
- [[Ruiz Scaling]]
- [[Primal-Dual Hybrid Gradient]]
- [[OLeary-1981-Equilibrating-Both-Matrices]]
- [[Oren-1980-Automatic-Scaling-Matrices]]

## Referenced By

- Architecture MOC
- Research MOC
- Research-Code Traceability MOC
- [[Presolve-Postsolve Stack|research/architectures/Presolve-Postsolve Stack]]
- [[Ill-Conditioning|research/concepts/Ill-Conditioning]]
- [[Numerical Stability|research/concepts/Numerical Stability]]
- [[Presolve|research/concepts/Presolve]]
- cross-paper-synthesis
- research-dependency-map
- [[Chinneck-1987-Primal-Dual-Methods|research/papers/Chinneck-1987-Primal-Dual-Methods]]
- [[Kallrath-2002-Planning-Scheduling-Industry|research/papers/Kallrath-2002-Planning-Scheduling-Industry]]
- [[Karmarkar-1984-New-Polynomial-Time-Algorithm|research/papers/Karmarkar-1984-New-Polynomial-Time-Algorithm]]
- [[Linan-2025-Trends-Perspectives-Deterministic|research/papers/Linan-2025-Trends-Perspectives-Deterministic]]
- [[Maros-2003-Computational-Optimization-Techniques|research/papers/Maros-2003-Computational-Optimization-Techniques]]
- [[McShane-1989-Implementation-Primal-Dual-Interior|research/papers/McShane-1989-Implementation-Primal-Dual-Interior]]
- [[OLeary-1981-Equilibrating-Both-Matrices|research/papers/OLeary-1981-Equilibrating-Both-Matrices]]
- [[Oren-1980-Automatic-Scaling-Matrices|research/papers/Oren-1980-Automatic-Scaling-Matrices]]
- [[Diagonal Preconditioning|research/techniques/Diagonal Preconditioning]]
- [[Harris Ratio Test|research/techniques/Harris Ratio Test]]
- [[Ruiz Scaling|research/techniques/Ruiz Scaling]]