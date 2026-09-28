---
type: paper
title: "An Approximate Minimum Degree Ordering Algorithm"
authors: "Amestoy, Davis & Duff"
year: 1996
venue: "SIMAX"
doi: "(unverified)"
domain: [sparse]
priority: ★
status: deep
tags: [paper, sparse]
---

# An Approximate Minimum Degree Ordering Algorithm

> Fast approximate-minimum-degree fill-reducing ordering — the default first choice before sparse factorization.

## Metadata
| Field | Value |
|---|---|
| Authors | Amestoy, Davis & Duff |
| Year | 1996 |
| Venue | SIMAX |
| DOI/URL | (unverified) |

## Problem Addressed
The sparsity of a factorization depends mostly on the elimination order, but exact minimum-degree computation is too expensive for large matrices. AMD computes a nearly-as-good ordering at a fraction of the cost using graph/quotient-graph approximations, so ordering time never dominates factorization time.

## Core Contribution
- **Methodology:** Quotient-graph representation with approximate degree bounds, supervariables (indistinguishable nodes), mass elimination, element absorption and aggressive absorption to keep degrees cheap to maintain.
- **Assumptions:** Structure (not values) determines fill; approximate degrees suffice; matrix treated as symmetric pattern for ordering purposes (approximate).
- **Benchmarks/datasets:** Standard sparse matrix collections of the 1990s (approximate; no instance list asserted).
- **Metrics:** nnz in the factor, ordering time, total factorization time.
- **Key results:** Near-minimum-degree quality at much lower ordering cost than exact MD, making it practical for very large problems (approximate; no figures asserted).

## Engineering-Relevant Knowledge
**Algorithms:** AMD ordering applied before sparse Cholesky/LU factorization.

**Techniques:** Fill-reducing ordering, quotient graph with supervariable merging, degree approximation with element absorption, mass elimination of degree-zero nodes.

**Implementation details:** Use before refactorizing a fixed pattern (basis patterns change only by column swaps) and before any normal-equation factorization; ordering can be computed once and cached per sparsity signature in src/linalg/.

**Equations/rules:** deg(v) ≈ |adj(v)| − (overlaps with adjacent elements); eliminate the node with smallest approximate degree, updating neighbors via pattern union.

**Limitations/failure cases:** Symmetric-pattern assumption makes it the wrong tool for strongly unsymmetric A (use COLAMD instead); degree approximation can misjudge dense rows/columns; ordering quality degrades on matrices with huge dense rows (approximate).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R6/R12 require keeping factorization sparse; AMD (or COLAMD) is the cheap lever for basis refactorization and any normal-equation path (R4's interior-point engine), though simplex basis patterns are already fairly good — measurement decides.

## Evidence → Engineering Decision
- *Finding:* Fill-reducing ordering reduces factor nnz at negligible cost when the pattern is fixed → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Ordering choice interacts with which factorization form is used (symmetric vs unsymmetric) → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error

## Related Papers
- [[Davis-2004-Column-Approximate-Minimum]]
- [[Gilbert-1988-Sparse-Partial-Pivoting]]
- [[Duff-1986-Direct-Methods-Sparse]]
- [[Karypis-1998-Fast-High-Quality]]

## Uses
- [[Fill-Reducing Ordering]]
- [[Sparsity]]
- [[Symbolic Factorization]]
