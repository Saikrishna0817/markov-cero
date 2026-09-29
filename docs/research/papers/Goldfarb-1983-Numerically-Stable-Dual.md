---
type: paper
title: "A Numerically Stable Dual Method for Solving Strictly Convex Quadratic Programs"
authors: "Goldfarb & Idnani"
year: 1983
venue: "Math. Prog."
doi: "10.1007/BF02591962"
domain: [qp]
priority: ★
status: deep
tags: [paper, qp]
---

# A Numerically Stable Dual Method for Solving Strictly Convex Quadratic Programs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Reference dual active-set QP algorithm: Cholesky + QR updates, no Phase I, numerical stability by construction.

## Metadata
| Field | Value |
|---|---|
| Authors | Goldfarb & Idnani |
| Year | 1983 |
| Venue | Math. Prog. |
| DOI/URL | 10.1007/BF02591962 |

## Problem Addressed
Existing QP methods either needed a feasible start (hard to get) or lacked stability guarantees. Goldfarb & Idnani solve the *dual* — which always has an interior starting point obtained from slack artificial constraints — and recover the primal from the active set.

## Core Contribution
- **Methodology:** Start from an artificial dual-feasible point (min x^T Q x subject to A x = 0 with slacks); take dual steps that violate constraints one at a time (most-violating / steepest edge selection); when dual feasibility breaks, restore it by moving back along a previously active constraint; at optimality, solve the equality-constrained QP on the active set for the primal.
- **Assumptions:** Q strictly positive definite (unique solution); constraints linear; exact rank of active constraint matrix (QR maintained).
- **Benchmarks/datasets:** Small/medium convex QPs of the era (paper's set).
- **Metrics:** Constraint violations, objective accuracy, number of active-set iterations.
- **Key results:** Numerically stable in practice; no Phase I needed; accuracy limited by update drift over long runs (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Dual active-set QP; equality-constrained QP solve on active set.
**Techniques:** Cholesky rank-1 updates/downdates; QR factorization of active constraints; dual start via artificial slacks.
**Implementation details:** markov-cero's QP engine is OSQP-style ADMM (`src/qp/admm_solver.cpp` + `src/qp/kkt.cpp` LDL^T) — active-set would be a second QP engine for small/medium problems where exact KKT satisfaction matters more than scaling.
**Equations/rules:** dual step picks most-violating constraint; feasibility restored via backtracking along active direction.
**Limitations/failure cases:** Iterations grow with the number of active constraints (bad for large constraint counts); requires strictly PD Q (see [[Connell-1999-Dual-Active-Set-Algorithm]] for PSD); update drift requires periodic refactorization.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R2 (QP in scope) + R17 (accuracy certificates). ADMM stops at a tolerance; a dual active-set gives exact active-set identification and tight [[Duality Gap]] on small QPs — useful for verification and MIQP node solves where the relaxation is small.

## Evidence → Engineering Decision
- *Finding:* ADMM accuracy is tolerance-bound while active-set methods converge to exact KKT points on small QPs → *PS requirement:* R2, R17 → *Component:* src/qp/admm_solver.cpp (engine selection) + src/qp/verifier.cpp → *Metric:* [[KKT Residual]] and [[Duality Gap]] reported per solve.
- *Finding:* Cholesky/QR update kernels are reusable sparse linear algebra → *PS requirement:* R6 → *Component:* src/linalg/dense_lu.cpp → *Metric:* factorization time per active-set iteration.

## Related Papers
- [[Goldfarb-1984-Dual-Primal-Dual-Methods]]
- [[Connell-1999-Dual-Active-Set-Algorithm]]
- [[Wright-1997-Primal-Dual-IPM]]
- [[Lawson-1974-Solving-Least-Squares]]

## Uses
- [[ADMM]]
- [[KKT Conditions]]
- [[Duality Gap]]
- [[QPLIB]]
