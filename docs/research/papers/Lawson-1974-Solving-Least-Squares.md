---
type: paper
title: "Solving Least Squares Problems; Practical Optimization"
authors: "Lawson & Hanson; Gill, Murray & Wright"
year: 1974
venue: "(not listed in source)"
doi: "(unverified)"
domain: [qp]
priority: ○
status: standard
tags: [paper, qp]
---
# Solving Least Squares Problems; Practical Optimization

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Two classical books: NNLS/least-squares algorithms and the practical toolbox (Cholesky handling, dogleg, QP subproblems) behind optimization codes.
## Metadata
| Field | Value |
|---|---|
| Authors | Lawson & Hanson (1974); Gill, Murray & Wright (1981) |
| Year | 1974 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |
## Problem Addressed
Equality-constrained subproblems, normal equations and least-squares fits recur in every solver (IPM Newton systems, active-set QP steps, regression heuristics). These books give the stable algorithms and their error analysis.
## Core Contribution
- **Methodology:** Lawson & Hanson: orthogonal-triangular decomposition methods for LS/NNLS with backward-stability arguments. Gill, Murray & Wright: practical optimization — Cholesky with condition estimation, trust-region/dogleg, QP handling, scaling.
- **Assumptions:** Linear least squares/equality constraints; well-posed or rank-revealing handling; floating-point error model.
- **Benchmarks/datasets:** Textbook examples and test problems.
- **Metrics:** Residual norms; solution accuracy; stability bounds.
- **Key results:** Standard algorithms still in use (NNLS active-set, Givens/Householder updates) (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Householder/Givens least squares; NNLS active-set; Cholesky factorization with condition estimation.
**Techniques:** Scaling before factorization; residual-driven refinement ([[Iterative Refinement]]).
**Implementation details:** `src/linalg/dense_lu.cpp` handles dense blocks today; these texts are the reference for stable dense kernels (SVD-less rank handling, condition estimates) used in QP active-set and IPM paths.
**Limitations/failure cases:** Rank-deficient LS needs pivoted QR or SVD; condition estimation must be cheap (O(n^2)) to run routinely.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R6 (efficient numerical linear algebra) and R9 — reference algorithms for dense subproblems inside QP/IPM; no direct deliverable in current code.
## Evidence → Engineering Decision
- *Finding:* Condition estimation before/after dense solves catches silent accuracy loss → *PS requirement:* R9, R17 → *Component:* src/linalg/dense_lu.cpp → *Metric:* condition estimate threshold; exit-7 rate.
## Related Papers
- [[Goldfarb-1983-Numerically-Stable-Dual]]
- [[Goldfarb-1984-Dual-Primal-Dual-Methods]]
## Uses
- [[Numerical Stability]]
