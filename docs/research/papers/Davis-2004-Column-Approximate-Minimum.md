---
type: paper
title: "COLAMD: A Column Approximate Minimum Degree Ordering Algorithm (Algorithm 836)"
authors: "Davis, Gilbert, Larimore & Ng"
year: 2004
venue: "ACM TOMS"
doi: "10.1145/1024074.1024080"
domain: [sparse]
priority: ★
status: deep
tags: [paper, sparse]
---

# COLAMD: A Column Approximate Minimum Degree Ordering Algorithm (Algorithm 836)

> Column-oriented approximate minimum degree ordering for unsymmetric matrices and normal-equation products.

## Metadata
| Field | Value |
|---|---|
| Authors | Davis, Gilbert, Larimore & Ng |
| Year | 2004 |
| Venue | ACM TOMS |
| DOI/URL | 10.1145/1024074.1024080 |

## Problem Addressed
AMD assumes a symmetric pattern, but LP constraint matrices and the normal equations of interior-point methods are unsymmetric, and what matters is fill in AᵀA or in the LU of A itself. COLAMD provides an approximate column minimum degree ordering designed precisely for those structures, cheap enough to run on every factorization.

## Core Contribution
- **Methodology:** Approximate column degrees computed from the column intersection structure (a proxy for the Cholesky factor of AᵀA), with supervariables and aggressive absorption as in AMD, but oriented to column elimination.
- **Assumptions:** Structure-only analysis; ordering quality judged by fill in the subsequent numeric factorization (approximate).
- **Benchmarks/datasets:** Unsparse/real-world unsymmetric collections of the early 2000s (approximate; no instance list asserted).
- **Metrics:** nnz in factor, ordering time, factorization time.
- **Key results:** Delivers near-minimum-degree column orderings for unsymmetric matrices; became the standard ordering shipped with SuiteSparse-style toolkits (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** COLAMD as pre-factorization ordering for unsymmetric LU and for normal equations (A D Aᵀ) in IPM/QP paths.

**Techniques:** Fill-reducing ordering, column intersection graphs, degree approximation with supervariable merging.

**Implementation details:** Natural fit for the interior-point/QP path (src/lp/first_order/pdlp.cpp, src/qp/) where normal equations are formed repeatedly on a fixed pattern — compute the ordering once per model; for the simplex basis (nearly square, pattern reused) benchmark COLAMD vs. no ordering before adopting.

**Equations/rules:** Order columns so that elimination of A's columns minimizes predicted fill in AᵀA; ordering is pattern-only, so it must be recomputed only when the sparsity pattern changes.

**Limitations/failure cases:** Column ordering does not exploit symmetric structure as well as AMD on symmetric problems; extremely dense columns confuse degree estimates; ordering quality does not guarantee better numerics (pivoting still governs stability).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R4 requires an interior-point engine (normal equations / LDLᵀ) and R6 requires sparse techniques; COLAMD-style ordering is the standard lever there, but its value for our simplex basis must be demonstrated by measurement (R16/R20) rather than assumed.

## Evidence → Engineering Decision
- *Finding:* Unsymmetric/normal-equation factorizations need column-oriented ordering → *PS requirement:* R6 → *Component:* src/lp/first_order/pdlp.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Ordering benefit must be shown per engine, not assumed → *PS requirement:* R16 → *Component:* benchmarks/ → *Metric:* Geometric Mean Runtime

## Related Papers
- [[Amestoy-1996-Approximate-Minimum-Degree]]
- [[Grigori-2007-Parallel-Symbolic-Factorization]]
- [[Ribizel-2023-Parallel-Symbolic-Cholesky]]
- [[Gilbert-1988-Sparse-Partial-Pivoting]]

## Uses
- [[Fill-Reducing Ordering]]
- [[Symbolic Factorization]]
