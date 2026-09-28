---
type: concept
tags: [algorithms, sparse]
status: stable
verified_on: 2026-09-25
---

# Markowitz Pivoting

> Choose the pivot minimizing (row count × column count) — the classic trade between fill-in and stability during sparse elimination.

## Definition
Markowitz (1957) selects an elimination pivot by scoring each candidate (i, j) with the product of the row count rᵢ and column count cⱼ of the candidate, since that product equals the number of new entries the elimination creates; minimizing it minimizes immediate fill-in. Pure Markowitz can pick a numerically terrible pivot, so production codes use *threshold* Markowitz: consider only candidates whose magnitude passes a stability threshold, then take the best Markowitz score among them (threshold partial pivoting when the row count is fixed at 1). The method operates on the current active submatrix, not the original sparsity graph, so it is a dynamic, per-elimination-step decision.

## Why It Matters Here
- R6/R9: fill decides sparse factorization cost and pivoting decides stability — Markowitz is the one rule that addresses both at once.
- Observed state: the repo's `SparseLu` picks largest |entry| in the column only, with no row/column count consideration (src/linalg/sparse_basis.cpp:120-135); docs/codebase/components/SparseBasis-LU.md records this as an open risk (fill can blow up before the nnz cap fires).
- Inference: adding Markowitz/threshold pivoting is a bounded change with direct impact on refactorization frequency and hence on MIP node cost.

## Key Facts / Rules
- Markowitz cost = rᵢ · cⱼ = number of fill entries created by eliminating pivot (i, j).
- Threshold variants: restrict candidates to |a_ij| ≥ u · max|a_·j| (stability floor), then minimize Markowitz cost.
- Update path: after an eta update, the "current" matrix for scoring differs from the base factorization — codes score on the updated column.
- Fill-reducing *ordering* (AMD) is a static, global analogue; Markowitz is the local dynamic version.

## Related
- [[Sparse LU]]
- [[Sparsity]]
- [[Fill-Reducing Ordering]]
- [[Numerical Stability]]
- [[Markowitz-1957-Elimination-Form-Inverse]]
- [[Suhl-1990-Fast-LU-Factorization]]

## Referenced By

- [[Algorithms MOC|research/Algorithms MOC]]
- [[Sparse LU|research/algorithms/Sparse LU]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Markowitz-1957-Elimination-Form-Inverse|research/papers/Markowitz-1957-Elimination-Form-Inverse]]
- [[Fill-Reducing Ordering|research/techniques/Fill-Reducing Ordering]]