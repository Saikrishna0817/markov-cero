---
type: paper
title: "Rounding Errors in Algebraic Processes / Moler iterative refinement"
authors: "Moler, 1967 (J. ACM); Higham, 1997 (IMA JNA)"
year: 1967
venue: "J. ACM; IMA JNA"
doi: "10.1145/321386.321394"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Rounding Errors in Algebraic Processes / Moler iterative refinement

> Moler's iterative refinement: compute a residual in extra precision and correct the solution — turning an inaccurate solve into an accurate one for ~1 LP solve' worth of cost.

## Metadata
| Field | Value |
|---|---|
| Authors | Moler, 1967 (J. ACM); Higham, 1997 (IMA J. Numer. Anal.) |
| Year | 1967 (first) |
| Venue | Journal of the ACM; IMA Journal of Numerical Analysis |
| DOI/URL | 10.1145/321386.321394 (Moler, per list) |

> **Audit flag:** the list's title column repeats Wilkinson's book title ("Rounding Errors in Algebraic Processes") for this row; the DOI is Moler's. Verify the true title of the Moler paper before citing — flagged as (unverified) title, verified DOI.

## Problem Addressed
A single LU solve is backward stable but its forward error scales with κ(A); on ill-conditioned LP bases the solution is then too inaccurate for reliable pivoting or certification.

## Core Contribution
- **Methodology:** Given an initial solution x̂, compute residual r = b − A x̂ in higher precision (or compensated arithmetic), solve A Δ = r with the existing factorization, and set x ← x̂ + Δ; iterate until the residual stops decreasing.
- **Assumptions:** Residual computed more accurately than the original solve (mixed/extended precision or double-double); κ(A)·ε < 1 (otherwise refinement cannot help).
- **Benchmarks/datasets:** Ill-conditioned linear systems / LP bases.
- **Metrics:** Residual reduction per sweep; achieved digits; termination test.
- **Key results:** Refinement recovers accuracy cheaply whenever κ is not catastrophic — standard in LAPACK and in LP crossover/certification paths.

## Engineering-Relevant Knowledge
**Algorithms:** Iterative refinement of basis solves (mixed-precision).
**Techniques:** Residual in higher precision; stopping on residual stagnation; using refinement results to *detect* failure (residual not reducing ⇒ matrix too ill-conditioned).
**Implementation details:** Directly relevant: solve with existing factors in `src/linalg/sparse_basis.cpp`, refine in `src/lp/dual/dual_simplex.cpp`, and use the residual as an objective failure detector feeding `src/verify/`. Also relevant to the GPU story: mixed-precision refinement is the standard way to make fast-but-imprecise kernels accurate (R8/R9).
**Equations/rules:** x_{k+1} = x_k + A⁻¹ (b − A x_k); stop when ‖r‖ no longer decreases or ‖r‖ ≤ tol·‖b‖.
**Limitations/failure cases:** Fails when κ·ε ≥ 1 (need exact arithmetic — see #155/#157); residual must really be computed with extra precision or refinement is a no-op.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** A small, high-value addition to our LP engine that improves accuracy *and* gives a built-in ill-conditioning signal — squarely within R9/R13/R17.

## Evidence → Engineering Decision
- *Finding:* no refinement step; accuracy unmeasured on ill-conditioned bases → *PS requirement:* R9, R13, R17 → *Component:* src/linalg/sparse_basis.cpp, src/lp/dual/dual_simplex.cpp → *Metric:* [[KKT Residual]], [[Numerical Error]]

## Related Papers
- [[Gleixner-2015-Iterative-Refinement-Linear]]
- [[Cline-1979-Estimate-Condition-Number]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic]]
- [[Gill-1974-Methods-Modifying-Matrix]]

## Uses
- [[Iterative Refinement]] [[Numerical Stability]] [[Ill-Conditioning]] [[Basis]]
