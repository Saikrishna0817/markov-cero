---
type: paper
title: "Primal-Dual IPM (Ch. on convex QP)"
authors: "Wright"
year: 1997
venue: "(not listed in source)"
doi: "(unverified)"
domain: [qp, ipm]
priority: ★
status: deep
tags: [paper, qp]
---

# Primal-Dual IPM (Ch. on convex QP)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Interior-point QP extending Mehrotra to convex quadratic programs (Wright 1997 chapter; lineage to Monteiro, Adler & Resende 1990).

## Metadata
| Field | Value |
|---|---|
| Authors | Wright (1997); cites Monteiro, Adler & Resende (1990) |
| Year | 1997 |
| Venue | (not listed in source) — chapter of SIAM book listed as Wright 1997 |
| DOI/URL | (unverified) |

## Problem Addressed
Active-set QP scales poorly with constraint count, while ADMM-style splitting converges slowly for high accuracy. The chapter shows the primal-dual IPM machinery carries over to convex QP by simply adding `P x` to the dual equations — same predictor-corrector, same linear algebra.

## Core Contribution
- **Methodology:** Central path for `min 1/2 x^T P x + q^T x s.t. A x = b, x >= 0`; KKT system becomes `[ -(P + D) A^T ; A 0 ]`; Mehrotra predictor-corrector applied unchanged; complementarity `x_i s_i = mu` where `s = q + P x + A^T y - z`.
- **Assumptions:** P positive semidefinite (strict convexity for uniqueness); strictly feasible/central start; accurate indefinite solves each iteration.
- **Benchmarks/datasets:** Convex QP test problems (chapter + Monteiro-Adler-Resende lineage).
- **Metrics:** Duality gap, residuals, iterations.
- **Key results:** O(sqrt(n))-class iteration behavior transfers from LP; high accuracy in modest iterations (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Interior-point convex QP (Mehrotra-style).
**Techniques:** Quasi-definite KKT factorization with regularization; same ordering/[[Sparsity]] machinery as LP IPM.
**Implementation details:** markov-cero already factorizes quasi-definite KKT systems (`src/qp/kkt.cpp`, sparse LDL^T) for ADMM — that kernel is the exact solver this chapter's Newton system needs, so IPM-QP would be mostly iteration logic.
**Equations/rules:** Newton system `[ -(X S + P) A^T ; A 0 ]` (normal-equation-free augmented form); step = min primal/dual positivity ratio.
**Limitations/failure cases:** Semidefinite P makes the KKT system singular on the null space (needs regularization); accuracy limited by factorization quality ([[Ill-Conditioning]]); needs crossover for MILP-compatible bases.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R2 (QP), R3 (MIQP extension), R9 (accuracy). Provides the high-accuracy alternative to ADMM for medium QPs and the QP-relaxation solve for MIQP nodes (`src/milp/node_lp.cpp` currently uses ADMM).

## Evidence → Engineering Decision
- *Finding:* Existing `src/qp/kkt.cpp` LDL^T kernel satisfies the IPM-QP Newton system requirements → *PS requirement:* R2, R6 → *Component:* src/qp/kkt.cpp + proposed src/qp/ipm.cpp → *Metric:* [[KKT Residual]] vs. iterations; solve time vs. ADMM.
- *Finding:* MIQP node QPs need consistent accuracy with node LPs → *PS requirement:* R3 → *Component:* src/milp/node_lp.cpp → *Metric:* [[Relative Optimality Gap]] at MIP termination.

## Related Papers
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Goldfarb-1983-Numerically-Stable-Dual]]
- [[Tits-1994-Simple-Quadratically-Convergent]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]

## Uses
- [[Interior-Point Method]]
- [[Sparse LDL Factorization]]
- [[KKT Conditions]]
- [[QPLIB]]
