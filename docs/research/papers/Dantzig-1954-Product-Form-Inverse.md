---
type: paper
title: "The Product Form for the Inverse in the Simplex Method"
authors: "Dantzig & Orchard-Hays"
year: 1954
venue: "MTAC"
doi: "10.1090/s0025-5718-1954-0061469-8"
domain: [lp, sparse]
priority: ★
status: deep
tags: [paper, lp, sparse]
---

# The Product Form for the Inverse in the Simplex Method

> PFI: carry B⁻¹ as a product of cheap elementary matrices instead of recomputing it every iteration.

## Metadata
| Field | Value |
|---|---|
| Authors | Dantzig & Orchard-Hays |
| Year | 1954 |
| Venue | MTAC |
| DOI/URL | 10.1090/s0025-5718-1954-0061469-8 |

## Problem Addressed
Recomputing the basis inverse from scratch at every simplex iteration is prohibitive; forming and storing an explicit updated inverse is equally bad. The paper shows how to represent the inverse as a product of elementary (eta) matrices — one per pivot — so an iteration applies a cheap structured update instead of a factorization.

## Core Contribution
- **Methodology:** Product-form representation B⁻¹ = Eₖ ⋯ E₂ E₁ B₀⁻¹, where each Eᵢ encodes a single column-replacement pivot, applied sequentially to solve systems.
- **Assumptions:** Basis remains nonsingular; eta factors remain well conditioned over a run (an assumption later work had to repair) (approximate).
- **Benchmarks/datasets:** Early LP computations (qualitative; no instance list asserted).
- **Metrics:** Operation count per iteration, accuracy over long update chains (qualitative).
- **Key results:** Makes the revised simplex computationally viable by reducing per-iteration inverse maintenance to structured applications (approximate).

## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex with product-form basis updates and periodic refactorization.

**Techniques:** Elementary-matrix (eta) updates, basis refactorization to a clean form, solve-by-successive-application.

**Implementation details:** PFI is the conceptual ancestor of our update path in src/linalg/sparse_basis.cpp; modern codes use Forrest-Tomlin/LU updates instead, but the same lifecycle applies — apply cheap updates for a window, then refactorize before error accumulates (see Bartels 1971).

**Equations/rules:** B₀⁻¹ updated by left-multiplying E per pivot; solve Bx = b as x ← E₁B₀⁻¹b, then E₂, … sequentially (Basis).

**Limitations/failure cases:** Long products of eta factors are slow to apply and numerically fragile — rounding drift grows with each factor, which is exactly why LU/FT updates superseded literal PFI (approximate).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R4/R9: understanding the PFI lifecycle explains *why* refactorization triggers exist and what an update path must guarantee — a design rationale we need even though we implement LU updates rather than literal PFI.

## Evidence → Engineering Decision
- *Finding:* Chained elementary updates accumulate rounding error → *PS requirement:* R9 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error
- *Finding:* Update-then-refactor is the original lifecycle pattern for basis maintenance → *PS requirement:* R4 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime

## Related Papers
- [[Dantzig-1963-Linear-Programming-Extensions]]
- [[Forrest-1972-Updating-Triangular-Factors]]
- [[Bartels-1969-Simplex-LU-Decomposition]]
- [[Bartels-1971-Stabilization-Simplex]]

## Uses
- [[Basis]] [[Sparse LU]] [[Numerical Stability]]
