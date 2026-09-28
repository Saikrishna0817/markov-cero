---
type: paper
title: "The Elimination Form of the Inverse and its Application to Linear Programming"
authors: "Markowitz"
year: 1957
venue: "Management Sci."
doi: "(unverified)"
domain: [sparse]
priority: ○
status: standard
tags: [paper, sparse]
---

# The Elimination Form of the Inverse and its Application to Linear Programming
> Origin of pivot selection that trades fill-in against numerical stability — the Markowitz criterion.
## Metadata
| Field | Value |
|---|---|
| Authors | Markowitz |
| Year | 1957 |
| Venue | Management Sci. |
| DOI/URL | (unverified) |
## Problem Addressed
Gaussian elimination pivots chosen purely for stability destroy sparsity, while pivots chosen purely for sparsity destroy accuracy; LP bases need a principled rule that prices both effects before each elimination step.
## Core Contribution
- **Methodology:** Derives the elimination form of the inverse and a pivot merit function combining predicted fill (row/column counts) with pivot magnitude.
- **Assumptions:** Sparsity pattern known symbolically; local greedy choice is an acceptable proxy for global fill (approximate).
- **Benchmarks/datasets:** Early LP and matrix problems (qualitative; no figures asserted).
- **Metrics:** Fill-in count, accuracy of the computed inverse/factors (qualitative).
- **Key results:** Provides the framework every sparse direct solver still uses for pivot selection (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Markowitz pivoting inside sparse LU factorization of the basis or constraint matrix.
**Techniques:** Markowitz count M(i,j) = (rᵢ − 1)(cⱼ − 1) minimized subject to a stability threshold on |pivot|.
**Implementation details:** In src/linalg/sparse_basis.cpp the practical form is threshold-constrained Markowitz: gather candidates, pick min-fill, fall back to partial pivoting when the candidate is below threshold — this policy must be measured, not assumed.
**Equations/rules:** pivot score = (row nnz − 1)·(col nnz − 1); accept only if |aᵢⱼ| ≥ τ·max_k|aₖⱼ| (threshold partial pivoting).
**Limitations/failure cases:** Exact Markowitz is expensive; greedy local choices can still produce poor orderings and instability on degenerate bases (approximate).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R6 (sparse linear algebra) and R9/R13 (stability on ill-conditioned models) meet exactly here: pivot policy is where sparsity and accuracy are traded, and it is a tunable we can ablate.
## Evidence → Engineering Decision
- *Finding:* Pivot policy is a measurable sparsity-vs-stability trade → *PS requirement:* R9 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error
## Related Papers
- [[Bartels-1969-Simplex-LU-Decomposition]] [[Suhl-1990-Fast-LU-Factorization]] [[Curtis-1972-Simplex-LU-Decomposition]]
## Uses
- [[Markowitz Pivoting]] [[Sparsity]]
