---
type: concept
tags: [concepts, sparse]
status: stable
verified_on: 2026-09-25
---

# Sparsity

> Industrial models are overwhelmingly zeros; algorithms that see that are the only ones that reach R12 scale.

## Definition
Sparsity is the property that only a small fraction of A's entries are nonzero, and it is what makes "thousands to millions of variables" tractable: storage, matrix-vector products and triangular solves all scale with nnz(A) rather than with dimension. Sparse format choice matters as much as the algorithm — CSC for column-oriented factorization and presolve, CSR for row-oriented kernels and GPU SpMV. The real cost driver inside a factorization is *fill*: new nonzeros created by elimination, which ordering tries to minimize and update/refactor policy tries to defer.

## Why It Matters Here
- R6 (sparse matrix techniques + numerical linear algebra) and R12 (industrial scale) are the same requirement seen from the algorithm and the data sides.
- Observed state: `SparseCanonicalModel` is CSC, assembled from triplets with duplicate-entry merging (src/transform/sparse_canonicalize.cpp:252-296); the GPU path uploads A and Aᵀ as CSR (gpu/src/pdhg_step.cpp:391-395).
- Observed state: the reference simplex is *dense* with caps 1024×8192 / 4M elements (src/lp/reference/revised_simplex.cpp:15-20) — Inference: that cap is exactly why it is labelled "reference" and why large models must reach the sparse engines.

## Key Facts / Rules
- Sparse triangular solve costs O(touched entries), not O(n²) (Gilbert–Peierls 1988) — provided pivoting keeps it sparse.
- Hyper-sparsity (Hall & McKinnon 2005): when FTRAN/BTRAN touch only a few entries, specialized loops give large speedups.
- Fill-in from elimination is bounded by ordering quality (AMD/COLAMD/METIS) before any numeric choice.
- Dense rows/columns in otherwise sparse models destroy the benefit — they must be prestructured or split (Howell 2018).

## Related
- [[Sparse LU]]
- [[Fill-Reducing Ordering]]
- [[Symbolic Factorization]]
- [[GPU CSR SpMV]]
- [[CSC Sparse Model]]
- [[Gilbert-1988-Sparse-Partial-Pivoting]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Research MOC|research/Research MOC]]
- [[Markowitz Pivoting|research/algorithms/Markowitz Pivoting]]
- [[Sparse LU|research/algorithms/Sparse LU]]
- [[CSC Sparse Model|research/architectures/CSC Sparse Model]]
- [[ED-004-sparse-first-canonicalization|research/engineering-decisions/ED-004-sparse-first-canonicalization]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Amestoy-1996-Approximate-Minimum-Degree|research/papers/Amestoy-1996-Approximate-Minimum-Degree]]
- [[Cheshmi-2017-Transforming-Sparse-Matrix|research/papers/Cheshmi-2017-Transforming-Sparse-Matrix]]
- [[Fourer-n.d.-Hierarchical-Solution-Large|research/papers/Fourer-n.d.-Hierarchical-Solution-Large]]
- [[Gamrath-2015-Progress-Presolving-Mixed|research/papers/Gamrath-2015-Progress-Presolving-Mixed]]
- [[Gilbert-1988-Sparse-Partial-Pivoting|research/papers/Gilbert-1988-Sparse-Partial-Pivoting]]
- [[Gondzio-1994-Another-Simplex-Type|research/papers/Gondzio-1994-Another-Simplex-Type]]
- [[Hall-1995-Asynchronous-Parallel-Revised|research/papers/Hall-1995-Asynchronous-Parallel-Revised]]
- [[Hall-2005-Hyper-sparsity-Revised-Simplex|research/papers/Hall-2005-Hyper-sparsity-Revised-Simplex]]
- [[Howell-2018-Prestructuring-Sparse-Matrices|research/papers/Howell-2018-Prestructuring-Sparse-Matrices]]
- [[Huangfu-2018-Parallelizing-Dual-Revised|research/papers/Huangfu-2018-Parallelizing-Dual-Revised]]
- [[Karypis-1998-Fast-High-Quality|research/papers/Karypis-1998-Fast-High-Quality]]
- [[Liu-1990-Role-Elimination-Trees|research/papers/Liu-1990-Role-Elimination-Trees]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector|research/papers/Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Markowitz-1957-Elimination-Form-Inverse|research/papers/Markowitz-1957-Elimination-Form-Inverse]]
- [[Martinson-1999-Interior-Point-Dantzig-Wolfe|research/papers/Martinson-1999-Interior-Point-Dantzig-Wolfe]]
- [[Neiro-2004-Mathematical-Modeling-Petroleum|research/papers/Neiro-2004-Mathematical-Modeling-Petroleum]]
- [[Ponte-2026-Good-Fast-Row-Sparse|research/papers/Ponte-2026-Good-Fast-Row-Sparse]]
- [[Unknown-2025-Parallelizing-Approximate-Minimum|research/papers/Unknown-2025-Parallelizing-Approximate-Minimum]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]
- [[Zhang-2026-Novel-Linear-Optimization|research/papers/Zhang-2026-Novel-Linear-Optimization]]
- [[Zhao-2025-Design-Implementation-Reduced|research/papers/Zhao-2025-Design-Implementation-Reduced]]
- [[Fill-Reducing Ordering|research/techniques/Fill-Reducing Ordering]]
- [[GPU CSR SpMV|research/techniques/GPU CSR SpMV]]
- [[Symbolic Factorization|research/techniques/Symbolic Factorization]]