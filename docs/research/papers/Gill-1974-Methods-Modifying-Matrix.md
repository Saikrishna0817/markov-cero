---
type: paper
title: "Methods for Modifying Matrix Factorizations"
authors: "Gill, Golub, Murray & Saunders"
year: 1974
venue: "J. Comp. Math."
doi: "(unverified)"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Methods for Modifying Matrix Factorizations

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> How to update an LU factorization after a rank-one change while controlling rounding error — the stable replacement for naive product-form-of-the-inverse updates.

## Metadata
| Field | Value |
|---|---|
| Authors | Gill, Golub, Murray & Saunders |
| Year | 1974 |
| Venue | "J. Comp. Math." as printed in list — **venue abbreviation ambiguous**; widely indexed as *Mathematics of Computation* (Math. Comp.). Verify before citing. |
| DOI/URL | (unverified) |

## Problem Addressed
The simplex method changes the basis one row/column at a time; refactorizing from scratch is expensive, but accumulating elementary products (PFI) in floating point silently destroys accuracy.

## Core Contribution
- **Methodology:** Classify rank-one modifications of triangular factors (column replacement, row replacement, and general rank-one), give algorithms for each with attention to element growth, and identify when updates must be abandoned in favor of refactorization.
- **Assumptions:** Existing LU; floating-point arithmetic; growth-factor monitoring.
- **Benchmarks/datasets:** Numerical test matrices and LP bases.
- **Metrics:** Element growth, rounding-error accumulation, factorization cost.
- **Key results:** Established the update-vs-refactorize decision as a *numerical policy* (not just a performance one) — the basis of every production simplex's updating scheme.

## Engineering-Relevant Knowledge
**Algorithms:** Rank-one LU updates; column/row replacement; growth monitoring; refactorization triggers.
**Techniques:** Track maximum element growth; refactorize when growth or error exceeds threshold; maintain sparsity during update.
**Implementation details:** `src/linalg/sparse_basis.cpp` implements our basis handling — this paper is the checklist: does it monitor growth? does it trigger refactorization? Audit item: basis-update policy not yet verified against these criteria (R9).
**Equations/rules:** Column replacement: solve triangular systems to express the new column in terms of the current factors; growth factor ρ = max|U_ij| / max|A_ij|.
**Limitations/failure cases:** Even stable updates accumulate error over thousands of pivots; growth can be exponential without pivoting — always pair with error estimation (#144) or refinement (#147).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** This is the numerical core of the LP engine (R6, R9, R13); its update-vs-refactorize policy is exactly the kind of "numerically robust" behavior the PS demands.

## Evidence → Engineering Decision
- *Finding:* basis update policy unverified for growth/refactorization triggers → *PS requirement:* R6, R9, R13 → *Component:* src/linalg/sparse_basis.cpp, src/lp/dual/dual_simplex.cpp → *Metric:* [[Numerical Error]], factorizations per solve

## Related Papers
- [[Bartels-1968-Numerical-Investigation-Simplex]]
- [[Cline-1979-Estimate-Condition-Number]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic]]
- [[Gleixner-2015-Iterative-Refinement-Linear]]

## Uses
- [[Basis]] [[Numerical Stability]] [[Iterative Refinement]] [[Dual Simplex]]
