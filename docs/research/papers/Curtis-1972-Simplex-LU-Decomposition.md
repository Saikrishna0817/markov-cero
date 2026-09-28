---
type: paper
title: "The Simplex Method of Linear Programming Using LU Decomposition"
authors: "Curtis & Reid"
year: 1972
venue: "JIMA"
doi: "(unverified)"
domain: [sparse, lp]
priority: ○
status: standard
tags: [paper, sparse, lp]
---

# The Simplex Method of Linear Programming Using LU Decomposition
> Represents the basis as LU factors and triangular solves instead of an explicitly maintained inverse.
## Metadata
| Field | Value |
|---|---|
| Authors | Curtis & Reid |
| Year | 1972 |
| Venue | JIMA |
| DOI/URL | (unverified) |
## Problem Addressed
Early simplex codes maintained the basis inverse (product form / explicit update), which is costly to apply and numerically fragile; the paper reformulates the simplex method so the basis is handled by sparse LU decomposition with triangular solves, the structure every modern simplex actually uses.
## Core Contribution
- **Methodology:** Derivation of revised-simplex linear algebra as sparse LU factorization of the basis plus forward/backward substitution, avoiding formation of B⁻¹.
- **Assumptions:** Basis is nonsingular; sparsity exploited during elimination order; refactorization when updates accumulate (approximate).
- **Benchmarks/datasets:** Small LPs of the early 1970s (qualitative; no numbers asserted).
- **Metrics:** Operation counts, fill-in, accuracy of computed steps (qualitative).
- **Key results:** LU-based basis handling beats product-form inverse on both speed and accuracy for sparse bases (approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex with LU basis representation; FTRAN/BTRAN triangular solves.
**Techniques:** Sparse elimination ordering, periodic refactorization, reuse of symbolic structure across iterations.
**Implementation details:** This is the blueprint for src/linalg/sparse_basis.cpp: pricing computes x̄ = B⁻¹Aⱼ and the ratio test computes ȳ = B⁻¹A_R by solves only — never invert; refactor when update count or fill thresholds trip.
**Equations/rules:** B = L·U·P; solve LUx = Pb by forward then backward substitution (FTRAN); adjoint solve for dual prices (BTRAN).
**Limitations/failure cases:** 1970s ordering heuristics; no threshold pivoting yet, so degenerate bases can still produce tiny pivots (see Suhl).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R4 (revised simplex), R6 (sparse linear algebra) and R9 (numerical stability) all land on this one representation; it is the interface contract between our simplex and our sparse kernels.
## Evidence → Engineering Decision
- *Finding:* Basis-as-LU with triangular solves is both faster and more accurate than inverse updates → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error
## Related Papers
- [[Bartels-1969-Simplex-LU-Decomposition]] [[Suhl-1990-Fast-LU-Factorization]] [[Gilbert-1988-Sparse-Partial-Pivoting]] [[Maros-2003-Computational-Optimization-Techniques]]
## Uses
- [[Sparse LU]] [[Numerical Stability]]
