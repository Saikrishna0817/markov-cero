---
type: paper
title: "Solving Dense Linear Systems with Semi-Normal Equations"
authors: "Fourer & Mehrotra"
year: "n.d."
venue: "(not listed in source)"
doi: "(unverified)"
domain: [ipm]
priority: ○
status: standard
tags: [paper, ipm]
---
# Solving Dense Linear Systems with Semi-Normal Equations

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> More robust way to run normal-equation solves inside optimization codes: compute residuals through a semi-normal-equation path.
## Metadata
| Field | Value |
|---|---|
| Authors | Fourer & Mehrotra |
| Year | n.d. (year not given in source list) |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |
## Problem Addressed
Normal equations `A D A^T` are the standard IPM/least-squares route but lose accuracy when A is ill-conditioned or nearly rank-deficient. Direct solves can return steps that are wrong without any obvious failure signal.
## Core Contribution
- **Methodology:** Semi-normal-equation solving (à la Stewart): form products via a residual-consistent order of operations so the computed solution's backward error is measured/controlled like a least-squares solve.
- **Assumptions:** Dense (or dense-reduced) systems; floating-point model where operation order is chosen for stability; factorization available.
- **Benchmarks/datasets:** Dense test systems of the late 1980s/early 1990s (qualitative).
- **Metrics:** Backward error / residual norm of the computed solve.
- **Key results:** More reliable residuals than naive normal-equation formation at modest extra cost (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Semi-normal equations; normal-equation least-squares solves.
**Techniques:** Residual computation during elimination; re-orthogonalization-style safeguards.
**Implementation details:** Relevant to dense blocks in markov-cero (`src/linalg/dense_lu.cpp`) and to Schur-complement updates in QP/IPM paths; a cheap guard when `cond(A)` is large.
**Limitations/failure cases:** Extra passes over data; does not fix structural rank deficiency — only exposes it.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R9/R13 (ill-conditioning demonstration) — semi-normal solves are a low-cost accuracy guard for dense reduced systems; today no such check exists in `src/linalg/dense_lu.cpp`.
## Evidence → Engineering Decision
- *Finding:* Naive normal-equation formation hides residual error → *PS requirement:* R9, R17 → *Component:* src/linalg/dense_lu.cpp (residual check after dense solve) → *Metric:* backward-error estimate; solver exit-7 rate.
## Related Papers
- [[Vanderbei-1995-Symmetric-Indefinite-Systems]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
## Uses
- [[Numerical Stability]]
