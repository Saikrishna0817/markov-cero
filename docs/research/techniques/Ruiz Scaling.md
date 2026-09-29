---
type: concept
tags: [techniques, numerics]
status: stable
verified_on: 2026-09-25
---

# Ruiz Scaling

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Iteratively multiply rows and columns by 1/√(norm) until everything is roughly size one — parameter-free equilibration.

## Definition
Ruiz–Torres equilibration scales a matrix so that both row and column 2-norms approach 1: at each iteration compute the current row and column norms, then multiply the existing scalers by δ_row = 1/√(row 2-norm) and δ_col = 1/√(column 2-norm), repeating until the maximum norm error falls below a tolerance (or an iteration cap). Because the update uses the *current* scaled matrix, the method self-corrects and needs no tuning of data-specific constants; it converges quickly for typical LP data. The scalers are stored so the exact inverse can be applied to primal solutions, duals, rays and certificates afterwards.

## Why It Matters Here
- R9/R13: preconditioning the model is the cheapest defense against ill-conditioning, and PDHG's convergence rate depends directly on norm balance.
- Observed state: `RuizOptions` default `max_iterations 10`, `tolerance 1e-3`, scales clamped to [1e-4, 1e4]; per-iteration δ = 1/√(norm) updates; `unscale_solution` restores primal, dual, rays and Farkas certificates (src/scale/ruiz_scaling.cpp:82-145).
- Observed state: applied only on the `primal`/`pdlp` CLI branches (apps/markov_cero_solve.cpp:293-301) and inside both PDLP engines — MILP/QP/parallel paths never scale (fact of absence in src/milp/*).

## Key Facts / Rules
- Update: D ← D · diag(1/√(row or column 2-norm of the currently scaled matrix), iterated.
- Convergence test: max |1 − scaled_norm| < tolerance on both rows and columns.
- Must unscale *both* primal (by column scales) and dual (inverse row scales) — asymmetry breaks duality checks.
- Unlike analytical max-scaling, Ruiz needs no per-instance parameters and is insensitive to outliers after a few passes.

## Related
- [[Scaling]]
- [[Ill-Conditioning]]
- [[Primal-Dual Hybrid Gradient]]
- RuizScaling
- [[OLeary-1981-Equilibrating-Both-Matrices]]

## Referenced By

- RuizScaling
- Solve-Pipeline
- Research MOC
- [[Primal-Dual Hybrid Gradient|research/algorithms/Primal-Dual Hybrid Gradient]]
- [[Scaling|research/concepts/Scaling]]
- cross-paper-synthesis
- [[Achterberg-2020-Presolve-Reductions-Mixed|research/papers/Achterberg-2020-Presolve-Reductions-Mixed]]
- [[Oren-1980-Automatic-Scaling-Matrices|research/papers/Oren-1980-Automatic-Scaling-Matrices]]
- [[Diagonal Preconditioning|research/techniques/Diagonal Preconditioning]]