---
type: paper
title: "Symmetric Indefinite Systems for Interior Point Methods"
authors: "Vanderbei; Fourer & Mehrotra"
year: 1995
venue: "(not listed in source)"
doi: "(unverified)"
domain: [ipm]
priority: ○
status: standard
tags: [paper, ipm]
---
# Symmetric Indefinite Systems for Interior Point Methods

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> LDL^T on a symmetric indefinite saddle system vs. normal equations: conditioning tradeoff for IPM linear algebra.
## Metadata
| Field | Value |
|---|---|
| Authors | Vanderbei (1995); Fourer & Mehrotra (1993) |
| Year | 1995 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |
## Problem Addressed
IPM linear systems can be written as normal equations `A Theta A^T` (SPD, Cholesky-friendly) or as a symmetric indefinite augmented system. Normal equations square the condition number; the augmented form is better conditioned but harder to factor. Which to choose?
## Core Contribution
- **Methodology:** Derivation and comparison of the two formulations; symmetric indefinite factorization with diagonal pivoting for the saddle-point system.
- **Assumptions:** Sparse A; ability to perform LDL^T with (partial) pivoting; bound/free-variable structure expressed via `Theta`.
- **Benchmarks/datasets:** LP test problems of the era (qualitative comparisons).
- **Metrics:** Condition number of the system actually factored; accuracy of Newton step.
- **Key results:** Augmented systems avoid kappa^2 blow-up but need pivoting/stabilization; normal equations are simpler and fast when A is well-conditioned (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** IPM Newton solve via symmetric indefinite LDL^T.
**Techniques:** Diagonal pivoting; regularization of small pivots; equilibration before factorization.
**Implementation details:** markov-cero already factorizes a quasi-definite KKT system for QP (`src/qp/kkt.cpp`, Davis LDL^T) — the same kernel is the natural home for an IPM saddle-point solve.
**Equations/rules:** normal equations have `cond = cond(A)^2`; augmented system preserves `cond(A)`-class conditioning (approximate statement).
**Limitations/failure cases:** Pivoting can destroy sparsity; irregular pivots near degeneracy ([[Degeneracy]]) require regularization heuristics.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Central design decision for R4's IPM requirement under R9 (numerical stability) and R6 (sparse linear algebra); informs whether to reuse `src/qp/kkt.cpp` kernels.
## Evidence → Engineering Decision
- *Finding:* Squared conditioning of normal equations loses accuracy on ill-conditioned rows → *PS requirement:* R9, R13 → *Component:* src/qp/kkt.cpp (LDL^T with diagonal pivot check, reuse for IPM saddle systems) → *Metric:* [[KKT Residual]] / [[Ill-Conditioning]] detection rate.
## Related Papers
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Fourer-n.d.-Solving-Dense-Linear]]
## Uses
- [[Sparse LDL Factorization]]
