---
type: paper
title: "A Stabilization of the Simplex Method"
authors: "Bartels"
year: 1971
venue: "Numer. Math."
doi: "(unverified)"
domain: [lp, numerics]
priority: ○
status: standard
tags: [paper, lp, numerics]
---

# A Stabilization of the Simplex Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> First rigorous rounding-error analysis of LU-based simplex, and a stabilization that follows from it.
## Metadata
| Field | Value |
|---|---|
| Authors | Bartels |
| Year | 1971 |
| Venue | Numer. Math. |
| DOI/URL | (unverified) |
## Problem Addressed
Sparse LU-based simplex (Bartels-Golub and successors) had been presented as an algorithm, but nobody had analyzed how floating-point elimination errors propagate through repeated basis updates; without that analysis there is no principled way to stabilize the method.
## Core Contribution
- **Methodology:** Rigorous rounding-error analysis of the LU simplex, then a stabilization scheme derived from where errors accumulate in factorization and update (approximate).
- **Assumptions:** Floating-point arithmetic model; repeated basis updates are the dominant error source (approximate).
- **Benchmarks/datasets:** Analytical examples of numerical breakdown (qualitative; no figures asserted).
- **Metrics:** Forward/backward error of basis solves, residual growth over iterations (qualitative).
- **Key results:** Identifies the error mechanisms in LU simplex and shows stabilization (periodic refactorization/controlled updates) restores accuracy (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** LU-based revised simplex with error-aware stabilization.
**Techniques:** Rounding-error monitoring, controlled update sequences, refactorization as error control.
**Implementation details:** Justifies the residual checks and refactor triggers in src/linalg/sparse_basis.cpp: analysis says drift is expected under indefinite updating, so verification must be routine rather than exceptional (feeds R17's robustness evidence).
**Equations/rules:** ‖ΔB‖ grows with number/size of updates; refactorization resets the error bound (Numerical Error).
**Limitations/failure cases:** 1971 arithmetic model predates modern BLAS/compiler behavior; analysis is conservative for well-scaled problems (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R9/R17 demand demonstrated numerical robustness; this gives the theoretical reason our code must verify factorization quality periodically — turning an implementation habit into an evidence-backed practice.
## Evidence → Engineering Decision
- *Finding:* Basis-update rounding error accumulates monotonically without refactorization → *PS requirement:* R17 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* KKT Residual
## Related Papers
- [[Bartels-1969-Simplex-LU-Decomposition]] [[Forrest-1972-Updating-Triangular-Factors]] [[Suhl-1990-Fast-LU-Factorization]]
## Uses
- [[Numerical Stability]] [[Iterative Refinement]]
