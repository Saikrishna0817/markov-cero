---
type: paper
title: "The Role of Elimination Trees in Sparse Factorization"
authors: "Liu"
year: 1990
venue: "SIMAX"
doi: "(unverified)"
domain: [sparse]
priority: ○
status: standard
tags: [paper, sparse]
---

# The Role of Elimination Trees in Sparse Factorization

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Elimination-tree theory: what determines the structure of L, and where parallelism lives.
## Metadata
| Field | Value |
|---|---|
| Authors | Liu |
| Year | 1990 |
| Venue | SIMAX |
| DOI/URL | (unverified) |
## Problem Addressed
Sparse factorization structure appears chaotic — rows of L seem to grow unpredictably — so predicting, storing and parallelizing factorization needs a compact structural theory rather than ad-hoc pattern simulation.
## Core Contribution
- **Methodology:** Graph-theoretic derivation of the elimination tree and its row subtrees, showing they completely characterize the nonzero pattern of L and enable parallel topological ordering.
- **Assumptions:** Symmetric-pattern (Cholesky-like) elimination model; values irrelevant to structure (approximate).
- **Benchmarks/datasets:** Sparse test matrices of the era (qualitative; no numbers asserted).
- **Metrics:** Predicted vs. actual nnz(L), parallelism available per tree level (qualitative).
- **Key results:** Row subtree of node i equals the nonzero structure of row i of L; tree depth bounds sequential dependence (approximate statement of the central results).
## Engineering-Relevant Knowledge
**Algorithms:** Symbolic factorization driven by elimination tree construction; parallel numeric factorization scheduling.
**Techniques:** Symbolic factorization, subtree-vs-supernode storage, topological scheduling of independent branches.
**Implementation details:** Relevant to any symbolic pass we add for refactorization reuse and to R7 parallelism: independent elimination-tree branches can be factorized concurrently; also used to detect dense columns early.
**Equations/rules:** Parent(j) = min{i > j : aᵢⱼ ≠ 0 in the reduced graph}; row subtree of i ⊆ {i} ∪ subtrees of children (Structure/Sparsity).
**Limitations/failure cases:** Tree theory assumes symmetric structure; unsymmetric LU needs the more complex elimination DAG (approximate); dense columns flatten the tree and kill parallelism.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R7 wants multi-core parallelization and R6 wants sparse technique; elimination trees are the cheap structural analysis that tells us whether a given factorization can be parallelized at all — worth knowing before promising GPU speedups (R8).
## Evidence → Engineering Decision
- *Finding:* Tree depth/shape predicts available parallelism and fill → *PS requirement:* R7 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Duff-1986-Direct-Methods-Sparse]] [[Amestoy-1996-Approximate-Minimum-Degree]] [[Grigori-2007-Parallel-Symbolic-Factorization]]
## Uses
- [[Symbolic Factorization]] [[Sparsity]]
