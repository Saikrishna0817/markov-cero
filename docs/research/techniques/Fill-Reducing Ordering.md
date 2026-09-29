---
type: concept
tags: [techniques, sparse]
status: stable
verified_on: 2026-09-25
---

# Fill-Reducing Ordering

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Permute before you eliminate: the single biggest lever on sparse factorization cost, and one this solver does not pull.

## Definition
A fill-reducing ordering is a permutation of rows/columns chosen before elimination so that the factorization creates as few new nonzeros as possible. Heuristics include minimum degree and its approximations (AMD, COLAMD for unsymmetric matrices), reverse Cuthill–McKee for bandwidth, and nested dissection (METIS) for large 2-D/3-D structure — all are graph-based and cheap relative to numeric factorization. The choice changes fill by orders of magnitude while leaving the mathematical solution untouched (it is applied to the factorization, not to the model). Ordering is computed once per sparsity pattern and reused by every subsequent numeric factorization of that pattern.

## Why It Matters Here
- R6/R12: fill drives memory and solve time on the large sparse models R11/R12 describe; without ordering, even a correct sparse LU degrades to near-dense on structured instances.
- Observed state: the QP KKT factorization stores `perm_/pinv_` documented as "identity if unused" with no AMD/COLAMD applied (include/markov_cero/qp/kkt.hpp:65-67); the basis LU performs no reordering at all (column-max pivoting only, src/linalg/sparse_basis.cpp:120-135).
- Inference: the missing ordering is the concrete mechanism behind the fill risks flagged in docs/codebase/components/LDL-Factorization.md and SparseBasis-LU.md.

## Key Facts / Rules
- AMD: repeatedly eliminate the vertex with (approximately) smallest degree, quotient-graph updating (Amestoy, Davis & Duff 1996).
- COLAMD: column-oriented approximate minimum degree, the standard for unsymmetric A and normal equations (Davis et al. 2004).
- Nested dissection (METIS) wins on large structured problems by recursive graph partitioning.
- Ordering affects fill only; numerical stability is governed by the separate pivoting rule (see [[Markowitz Pivoting]]).

## Related
- [[Symbolic Factorization]]
- [[Sparse LU]]
- [[Sparsity]]
- [[Markowitz Pivoting]]
- [[Amestoy-1996-Approximate-Minimum-Degree]]
- [[Davis-2004-Column-Approximate-Minimum]]

## Referenced By

- 15-roadmap
- 21-traceability
- Algorithms MOC
- Architecture MOC
- Research MOC
- [[Markowitz Pivoting|research/algorithms/Markowitz Pivoting]]
- [[Sparse LDL Factorization|research/algorithms/Sparse LDL Factorization]]
- [[Sparse LU|research/algorithms/Sparse LU]]
- [[CSC Sparse Model|research/architectures/CSC Sparse Model]]
- [[Sparsity|research/concepts/Sparsity]]
- cross-paper-synthesis
- [[Amestoy-1996-Approximate-Minimum-Degree|research/papers/Amestoy-1996-Approximate-Minimum-Degree]]
- [[Davis-2004-Column-Approximate-Minimum|research/papers/Davis-2004-Column-Approximate-Minimum]]
- [[Duff-1986-Direct-Methods-Sparse|research/papers/Duff-1986-Direct-Methods-Sparse]]
- [[Karypis-1998-Fast-High-Quality|research/papers/Karypis-1998-Fast-High-Quality]]
- [[Ribizel-2023-Parallel-Symbolic-Cholesky|research/papers/Ribizel-2023-Parallel-Symbolic-Cholesky]]
- [[Zhao-2025-Design-Implementation-Reduced|research/papers/Zhao-2025-Design-Implementation-Reduced]]
- [[Symbolic Factorization|research/techniques/Symbolic Factorization]]