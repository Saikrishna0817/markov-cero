---
type: paper
title: "On Implementing Mehrotra's Predictor-Corrector IPM for LP"
authors: "Lustig, Marsten & Shanno"
year: 1992
venue: "SIAM J. Optim."
doi: "(unverified)"
domain: [ipm]
priority: ★
status: deep
tags: [paper, ipm]
---

# On Implementing Mehrotra's Predictor-Corrector IPM for LP

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Catalogue of the implementation pitfalls of Mehrotra's algorithm: free variables, bound handling, dense columns and Schur-complement instability.

## Metadata
| Field | Value |
|---|---|
| Authors | Lustig, Marsten & Shanno |
| Year | 1992 |
| Venue | SIAM J. Optim. |
| DOI/URL | (unverified) |

## Problem Addressed
Publishing an algorithm is not the same as making it run: Mehrotra's scheme left many choices open (how to handle free variables, explicit bounds, how to update `mu`, which linear system to form). The paper reports what actually works on large LPs and what breaks.

## Core Contribution
- **Methodology:** Systematic implementation study of predictor-corrector on standard-form and bounded LPs; compares normal-equations vs. symmetric augmented (indefinite) systems; proposes practical heuristics for `mu` and step filtering.
- **Assumptions:** Sparse LP data; quasi-definite augmented systems factorable by LDL^T with stabilization; iterates kept strictly feasible by step limiting.
- **Benchmarks/datasets:** Netlib LP collection (large, sparse, varied conditioning).
- **Metrics:** Iterations, factorization time, accuracy reached, robustness across the test set.
- **Key results:** Robust performance across Netlib when bounds are handled as first-class structure; dense columns and poorly ordered Schur complements dominate failure modes (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Mehrotra predictor-corrector with practical `mu` heuristics; symmetric indefinite (saddle-point) system option.
**Techniques:** Splitting free variables into positive/negative pairs; treating simple bounds inside the KKT system; dense-column detection before factorization.
**Implementation details:** Ordering (COLAMD/AMD) materially changes fill and runtime; dense columns should be moved out or regularized; markov-cero's sparse model (`src/transform/sparse_canonicalize.cpp`) can carry a dense-column flag cheaply.
**Equations/rules:** augmented system `[ -(1/Theta) A^T ; A ]` quasi-definite → LDL^T without pivoting blow-up (with diagonal perturbation as fallback).
**Limitations/failure cases:** Schur-complement update from a dense column is O(m^2); unregularized systems stall near degeneracy ([[Degeneracy]]); heuristics are tuned, not proved.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Directly relevant to R4 + R6 + R9 if/when an IPM is added; today none of this code exists. The dense-column warning also applies to PDLP's normal-equation-style preconditioning and to `src/qp/kkt.cpp`, which already factorizes a quasi-definite KKT system with sparse LDL^T.

## Evidence → Engineering Decision
- *Finding:* Dense columns wreck Schur-complement cost and stability → *PS requirement:* R6, R12 → *Component:* src/transform/sparse_canonicalize.cpp (column density statistics) → *Metric:* factorization time share, [[Sparsity]]-aware nnz after ordering.
- *Finding:* Quasi-definite augmented systems are the robust alternative to squared normal equations → *PS requirement:* R9, R13 → *Component:* src/qp/kkt.cpp (existing LDL^T KKT path to generalize) → *Metric:* [[KKT Residual]] after solve; condition-number estimate.

## Related Papers
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Vanderbei-1995-Symmetric-Indefinite-Systems]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Lustig-1994-Interior-Point-Methods]]

## Uses
- [[Interior-Point Method]]
- [[Sparse LDL Factorization]]
- [[Ill-Conditioning]]
- [[Numerical Stability]]
