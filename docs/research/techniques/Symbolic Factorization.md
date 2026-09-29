---
type: concept
tags: [techniques, sparse]
status: stable
verified_on: 2026-09-25
---

# Symbolic Factorization

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Decide the fill pattern from the sparsity graph alone, then run numbers into it — the phase that makes repeated factorizations affordable.

## Definition
Symbolic factorization determines, without touching numeric values, the nonzero pattern of the factors L (or L and U) produced by eliminating a given sparsity graph under a given ordering — computed via elimination trees, column counts and row subtrees. It runs once per (matrix graph, ordering) pair and is reused for every numeric factorization of matrices sharing that pattern, which is why adaptive-ρ ADMM or refactorizing simplex bases can skip it. Its outputs also give memory requirements in advance, letting a solver allocate factor storage exactly once. Cost is graph-theoretic (O(nnz · α) class algorithms), independent of the numbers.

## Why It Matters Here
- R6 (sparse numerical linear algebra): the two-phase split is the standard engineering pattern for any solver that refactorizes often.
- Observed state: `ldl_symbolic` (src/qp/kkt.cpp:13) runs before `ldl_numeric` (:52) in `KktSolver::factorize`; `update_numeric` re-runs only the numeric phase when ρ/σ change (src/qp/kkt.cpp:226-263) — the pattern is implemented, if only for the QP KKT path.
- Inference: the basis-factorization path (`SparseLu`) has no separate symbolic phase — Inference: pattern is recomputed implicitly during each numeric elimination.

## Key Facts / Rules
- Input: sparsity graph + ordering; output: column counts, pattern of L, elimination tree, workspace sizes.
- Numeric phase is then a pure fill-in of the predetermined pattern — no dynamic growth decisions needed.
- Symbolic analysis is invalidated by any change to the graph or ordering (not by value changes).
- Parallel symbolic factorization (Grigori et al. 2007; Ribizel & Anzt 2023) is the scalable variant for very large systems.

## Related
- [[Sparse LDL Factorization]]
- [[Fill-Reducing Ordering]]
- [[Sparsity]]
- LDL-Factorization
- [[Grigori-2007-Parallel-Symbolic-Factorization]]

## Referenced By

- Architecture MOC
- Research MOC
- [[Sparse LDL Factorization|research/algorithms/Sparse LDL Factorization]]
- [[Sparsity|research/concepts/Sparsity]]
- [[Amestoy-1996-Approximate-Minimum-Degree|research/papers/Amestoy-1996-Approximate-Minimum-Degree]]
- [[Cheshmi-2017-Transforming-Sparse-Matrix|research/papers/Cheshmi-2017-Transforming-Sparse-Matrix]]
- [[Davis-2004-Column-Approximate-Minimum|research/papers/Davis-2004-Column-Approximate-Minimum]]
- [[Duff-1986-Direct-Methods-Sparse|research/papers/Duff-1986-Direct-Methods-Sparse]]
- [[Grigori-2007-Parallel-Symbolic-Factorization|research/papers/Grigori-2007-Parallel-Symbolic-Factorization]]
- [[Liu-1990-Role-Elimination-Trees|research/papers/Liu-1990-Role-Elimination-Trees]]
- [[Lustig-1994-Interior-Point-Methods|research/papers/Lustig-1994-Interior-Point-Methods]]
- [[Ribizel-2023-Parallel-Symbolic-Cholesky|research/papers/Ribizel-2023-Parallel-Symbolic-Cholesky]]
- [[Fill-Reducing Ordering|research/techniques/Fill-Reducing Ordering]]