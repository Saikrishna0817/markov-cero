---
type: concept
tags: [algorithms, sparse]
status: stable
verified_on: 2026-09-25
---

# Sparse LU

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Eliminate the basis column by column, pivoting for stability and watching fill — the linear algebra every simplex iteration stands on.

## Definition
Sparse LU factorizes a square sparse matrix B = P L U with Gaussian elimination restricted to stored nonzeros: rows/columns are permuted so that elimination touches only existing structure, and a pivoting rule chooses among candidate pivots to balance stability against fill-in. In simplex the factorization is reused across iterations via rank-one eta (Product-Form-Of-Inverse) updates and redone when updates accumulate fill or degrade accuracy — "refactorization policy" is as important as the factorization itself. Sparse triangular solves (FTRAN/BTRAN) must themselves remain sparse, which requires pivoting that does not scatter the column (Gilbert–Peierls 1988 gives the proportional-cost result).

## Why It Matters Here
- R6 (efficient numerical linear algebra) and R9 (stability) meet exactly here; every LP/MILP node cost is dominated by these solves.
- Observed state: `SparseLu::factorize` eliminates column-by-column with partial pivoting = largest |entry| in the current column, throws `"singular sparse basis"` at `singular_tolerance` 1e-14, and records min/max |pivot|, nnz and growth factor (src/linalg/sparse_basis.cpp:81-143); updates are `Eta` records with `maximum_updates 64`, `eta_density_trigger 0.5`.
- Inference (open question in docs/codebase/components/SparseBasis-LU.md): pivoting is column-max only with no Markowitz row/column-count consideration, so fill can grow before the nnz cap fires.

## Key Facts / Rules
- Partial pivoting by largest |entry| bounds growth but ignores fill; Markowitz pivoting targets fill directly.
- Update vs refactor: eta updates are O(nnz of update column); refactorization restores sparsity and conditioning.
- Growth factor (max |u_ij| / max |b_ij|) is the practical forward-error predictor for the factorization.
- Threshold partial pivoting (Suhl & Suhl 1990) is the production compromise: accept any pivot above a threshold, choose the sparsest among them.

## Related
- [[Basis]]
- [[Markowitz Pivoting]]
- [[Sparsity]]
- [[Fill-Reducing Ordering]]
- SparseBasis-LU
- [[Suhl-1990-Fast-LU-Factorization]]

## Referenced By

- 21-traceability
- SparseBasis-LU
- Algorithms MOC
- Research MOC
- [[Markowitz Pivoting|research/algorithms/Markowitz Pivoting]]
- [[Basis|research/concepts/Basis]]
- [[Sparsity|research/concepts/Sparsity]]
- cross-paper-synthesis
- [[Amestoy-2000-Parallel-Sparse-Linear|research/papers/Amestoy-2000-Parallel-Sparse-Linear]]
- [[Anderson-1989-Solving-Sparse-Linear|research/papers/Anderson-1989-Solving-Sparse-Linear]]
- [[Bartels-1969-Simplex-LU-Decomposition|research/papers/Bartels-1969-Simplex-LU-Decomposition]]
- [[Bixby-2002-Evolution-of-LP|research/papers/Bixby-2002-Evolution-of-LP]]
- [[Curtis-1972-Simplex-LU-Decomposition|research/papers/Curtis-1972-Simplex-LU-Decomposition]]
- [[Dantzig-1954-Product-Form-Inverse|research/papers/Dantzig-1954-Product-Form-Inverse]]
- [[Duff-1986-Direct-Methods-Sparse|research/papers/Duff-1986-Direct-Methods-Sparse]]
- [[Forrest-1972-Updating-Triangular-Factors|research/papers/Forrest-1972-Updating-Triangular-Factors]]
- [[Gilbert-1988-Sparse-Partial-Pivoting|research/papers/Gilbert-1988-Sparse-Partial-Pivoting]]
- [[Gleixner-2012-Factorization-Update-Reduced|research/papers/Gleixner-2012-Factorization-Update-Reduced]]
- [[Reid-1982-Sparsity-Exploiting-Variant|research/papers/Reid-1982-Sparsity-Exploiting-Variant]]
- [[Suhl-1990-Fast-LU-Factorization|research/papers/Suhl-1990-Fast-LU-Factorization]]
- [[Fill-Reducing Ordering|research/techniques/Fill-Reducing Ordering]]