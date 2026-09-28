---
type: paper
title: "Direct Methods for Sparse Matrices"
authors: "Duff, Erisman & Reid"
year: 1986
venue: "Book"
doi: "(unverified)"
domain: [sparse]
priority: ○
status: standard
tags: [paper, sparse]
---

# Direct Methods for Sparse Matrices
> Complete reference for sparse direct factorization: graphs, elimination trees, ordering, storage and stability.
## Metadata
| Field | Value |
|---|---|
| Authors | Duff, Erisman & Reid |
| Year | 1986 (approximate — first edition; list gives no year) |
| Venue | Book |
| DOI/URL | (unverified) |
## Problem Addressed
Sparse Gaussian elimination is taught as if matrices were dense; practitioners need one rigorous account of how graph structure, elimination orderings, data structures and numerical stability interact in real sparse factorizations.
## Core Contribution
- **Methodology:** Book deriving sparse LU/Cholesky from graph models: elimination graphs, fill prediction, orderings (minimum degree, nested dissection), storage schemes and error analysis.
- **Assumptions:** Direct methods (not iterative); structure-dominated runtime (approximate).
- **Benchmarks/datasets:** Standard sparse collections (Harwell-Boeing era) (qualitative).
- **Metrics:** nnz in factors, flop counts, backward error (qualitative).
- **Key results:** The canonical synthesis that later codes (and later papers in this module) build on (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Sparse LU/Cholesky factorization, triangular solves, ordering algorithms.
**Techniques:** Fill-reducing ordering, symbolic/numeric factorization separation, compressed sparse storage, error bounds for triangular solves.
**Implementation details:** Supplies the vocabulary and invariants for src/linalg/sparse_basis.cpp — e.g., when a symbolic pass may be reused and how to bound solve error — and is the background needed to read every other Module 2 note.
**Equations/rules:** Forward error of triangular solve grows with cond(L)·cond(U) and unit roundoff (Numerical Error); fill predicted on the elimination graph.
**Limitations/failure cases:** Book-scale treatment; no LP-specific pivoting policy or MIP-node reuse guidance.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R6/R9 require us to make correct, defensible sparse-linear-algebra choices from scratch (R10); this is the derivation source for those choices, though it ships no code we may reuse.
## Evidence → Engineering Decision
- *Finding:* Symbolic structure can be computed once and reused across numeric refactorizations → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Amestoy-1996-Approximate-Minimum-Degree]] [[Liu-1990-Role-Elimination-Trees]] [[Bartels-1969-Simplex-LU-Decomposition]]
## Uses
- [[Sparse LU]] [[Symbolic Factorization]] [[Fill-Reducing Ordering]]
