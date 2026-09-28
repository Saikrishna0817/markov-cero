---
type: paper
title: "The Simplex Method of Linear Programming Using LU Decomposition"
authors: "Bartels & Golub"
year: 1969
venue: "CACM"
doi: "10.1145/362946.362974"
domain: [sparse, lp]
priority: ★
status: deep
tags: [paper, sparse, lp]
---

# The Simplex Method of Linear Programming Using LU Decomposition

> Carries the LP basis as an LU decomposition with triangular solves instead of an explicitly maintained inverse.

## Metadata
| Field | Value |
|---|---|
| Authors | Bartels & Golub |
| Year | 1969 |
| Venue | CACM |
| DOI/URL | 10.1145/362946.362974 |

## Problem Addressed
Early simplex implementations maintained the basis inverse through product-form or explicit inverse updates, which are expensive to apply and numerically fragile as updates accumulate. The paper reformulates simplex linear algebra as Gaussian elimination on the basis — factor once, solve by substitution, update or refactor as pivots arrive.

## Core Contribution
- **Methodology:** Express the basis B as an LU decomposition; obtain all simplex quantities (basic solution, dual prices, tableau column for pricing, ratio-test column) from forward/backward substitution against the factors rather than from B⁻¹.
- **Assumptions:** Basis is nonsingular at every step; accuracy depends on pivoting strategy; sparse-structure concerns were secondary in 1969 (approximate).
- **Benchmarks/datasets:** Classic small LPs of the late 1960s (qualitative; no instance list asserted).
- **Metrics:** Operation counts per iteration and accuracy of computed steps (qualitative).
- **Key results:** LU-based basis handling is practical and more accurate than inverse maintenance; this became the universal representation for every serious simplex code (qualitative/approximate).

## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex (primal and dual) built on LU factorization of the basis.

**Techniques:** Rank-1 basis updates, periodic refactorization, partial/selection pivoting, FTRAN/BTRAN triangular solves.

**Implementation details:** This is the contract for src/linalg/sparse_basis.cpp: pricing computes B⁻¹Aⱼ by a forward solve, the ratio test uses B⁻¹b, dual prices come from an adjoint (backward) solve — no explicit inverse is ever formed; refactor when fill or error thresholds trip.

**Equations/rules:** B = L·U; basic solution from LUx = b; duals from Uᵀz = c_B; a column replacement is a rank-1 update handled by elimination operations or refactorization.

**Limitations/failure cases:** Dense-era formulation with no fill-reducing ordering; without threshold pivoting, tiny pivots on degenerate bases amplify rounding error — later work (Suhl, Forrest-Tomlin) patches both gaps.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It specifies the exact linear-algebra API between our simplex engines (R4) and sparse kernels (R6), and it is the first place where basis stability (R9) enters the design; everything else in Module 2 refines this representation.

## Evidence → Engineering Decision
- *Finding:* Basis-as-LU with triangular solves replaces inverse maintenance for all simplex quantities → *PS requirement:* R4 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error
- *Finding:* Refactorization threshold must be chosen empirically per model class → *PS requirement:* R17 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime

## Related Papers
- [[Curtis-1972-Simplex-LU-Decomposition]]
- [[Forrest-1972-Updating-Triangular-Factors]]
- [[Gilbert-1988-Sparse-Partial-Pivoting]]
- [[Suhl-1990-Fast-LU-Factorization]]

## Uses
- [[Sparse LU]]
- [[Basis]]
- [[Iterative Refinement]]
