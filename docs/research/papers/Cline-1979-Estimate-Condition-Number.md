---
type: paper
title: "An Estimate for the Condition Number of a Matrix"
authors: "Cline, Moler, Stewart & Wilkinson"
year: 1979
venue: "SINUM"
doi: "10.1137/0716029"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# An Estimate for the Condition Number of a Matrix

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> O(n²) estimator of ‖A‖·‖A⁻¹‖ using a few solves with A and Aᵀ — cheap enough to run on every basis, so ill-conditioning is *detected* instead of discovered as garbage output.

## Metadata
| Field | Value |
|---|---|
| Authors | Cline, Moler, Stewart & Wilkinson |
| Year | 1979 |
| Venue | SIAM Journal on Numerical Analysis (list: "SINUM") |
| DOI/URL | 10.1137/0716029 |

## Problem Addressed
Exact condition numbers cost as much as the inverse itself, so solvers ran blindly until results were meaningless. A cheap, reliable *estimate* would let a solver warn, refactorize or switch strategy on ill-conditioned systems.

## Core Contribution
- **Methodology:** Estimate κ(A) = ‖A‖‖A⁻¹‖ by solving A x = b and Aᵀ y = x for a few vectors b, then forming ‖x‖‖y‖/‖b‖² (Hager/Higham-style estimator in later refinement); work is a few triangular solves — negligible next to one factorization.
- **Assumptions:** Available factorization (so solves are cheap); norm choice specified (1- or ∞-norm).
- **Benchmarks/datasets:** Numerical linear algebra test matrices.
- **Metrics:** Quality of the κ estimate vs. exact; cost per estimate.
- **Key results:** Makes conditioning a *runtime observable* — the standard approach in LAPACK (xLACON) and the direct answer to "detect ill-conditioning early".

## Engineering-Relevant Knowledge
**Algorithms:** Condition-number estimation via norm-maximizing vector search (Hager's method as implemented by Cline et al.).
**Techniques:** Run on the current [[Basis]] after factorization/update; escalate: warn → refactorize → iterative refinement → fail with certificate.
**Implementation details:** Requires only FTRAN/BTRAN solves we already have (`src/linalg/sparse_basis.cpp`). Natural home: a κ check inside `src/lp/dual/dual_simplex.cpp`, feeding a status into `src/verify/`. PS calls for ill-conditioned matrices (R13) — this is the detection half.
**Equations/rules:** κ̂(A) ≈ ‖x‖·‖y‖ / (‖b‖·‖x‖) style estimator from solves; compare κ̂·ε_machine against a threshold (e.g. 1/ε) to flag breakdown.
**Limitations/failure cases:** An *estimate* can under-estimate badly (it is a lower bound in theory); must be paired with an accuracy check on the actual solution (residual), not trusted alone.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Cheap, implementable from existing components, and converts "ill-conditioned" from a failure mode into a measured signal for R13/R17.

## Evidence → Engineering Decision
- *Finding:* no condition estimation in our LP path → *PS requirement:* R9, R13, R17 → *Component:* src/linalg/sparse_basis.cpp, src/lp/dual/dual_simplex.cpp → *Metric:* [[Ill-Conditioning]] rate, [[KKT Residual]]

## Related Papers
- [[Gill-1974-Methods-Modifying-Matrix]]
- [[Gleixner-2015-Iterative-Refinement-Linear]]
- [[Renegar-1994-Condition-Numbers-Linear]]
- [[Maros-0000-New-Degeneracy-Method]]

## Uses
- [[Ill-Conditioning]] [[Numerical Stability]] [[Basis]] [[Iterative Refinement]]
