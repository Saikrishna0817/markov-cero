---
type: paper
title: "On the Implementation of a Primal-Dual Interior Point Algorithm"
authors: "Mehrotra"
year: 1992
venue: "SIAM J. Optim."
doi: "10.1137/0802015"
domain: [ipm]
priority: ★
status: deep
tags: [paper, ipm]
---

# On the Implementation of a Primal-Dual Interior Point Algorithm

> Predictor-corrector IPM — the algorithmic kernel used, with variations, by essentially every modern commercial interior-point LP solver.

## Metadata
| Field | Value |
|---|---|
| Authors | Mehrotra |
| Year | 1992 |
| Venue | SIAM J. Optim. |
| DOI/URL | 10.1137/0802015 |

## Problem Addressed
Pure path-following primal-dual methods required small conservative steps and many iterations to reach high accuracy. Mehrotra asked how to exploit second-order information so each iteration makes a large, still-safe progress. The result replaced short-step theory with a practical, robust scheme.

## Core Contribution
- **Methodology:** Predictor step along the affine-scaling direction (mu = 0) to estimate achievable progress, then a corrector step targeting the new complementarity `x s = mu e`; update `mu` from actual complementarity (`mu = x^T s / n`) scaled by a power of the predictor ratio; step length by primal/dual ratio test.
- **Assumptions:** Standard-form LP with positive iterates; one symmetric positive-definite (or quasi-definite) system solved per iteration to sufficient accuracy.
- **Benchmarks/datasets:** Netlib LPs (large sparse test set) in the computational literature built on this scheme.
- **Metrics:** Iterations to `1e-8` duality gap; factorization count; wall time.
- **Key results:** Roughly O(sqrt(n)) iterations to high accuracy in practice, about half the iterations of pure path-following (qualitative/approximate).

## Engineering-Relevant Knowledge
**Algorithms:** Predictor-corrector primal-dual IPM; step-length ratio tests.
**Techniques:** Adaptive `mu` update from step quality; centrality correction; sparse Cholesky/LDL^T of the normal or augmented system.
**Implementation details:** Schur complement `A Theta A^T` must be factored with fill-reducing ordering; dense columns blow up the factor — flagged by Lustig et al. 1992. markov-cero has no `src/lp/ipm/` yet; PDLP is the only non-simplex LP engine.
**Equations/rules:** predictor step `Delta_aff`; corrector adds `(sigma*mu - Delta_x_aff Delta_s_aff)/(x)` diagonal shift; final step = min(alpha_p, alpha_d) * safety.
**Limitations/failure cases:** Squared conditioning of normal equations loses ~2x digits; near-degenerate LPs cause tiny steps (ties to [[Degeneracy]]); requires good sparse linear algebra to beat first-order methods.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R4 explicitly requires interior-point methods; this is the specification-level algorithm to implement. Today's PDLP engine covers accuracy-insensitive large LPs, but R17 (numerical robustness demonstration) and crossover to a basis both assume an IPM exists — it does not.

## Evidence → Engineering Decision
- *Finding:* Predictor-corrector roughly halves iteration counts vs. pure path-following → *PS requirement:* R4 → *Component:* src/lp/ipm/ (proposed; currently absent — see src/lp/first_order/pdlp.cpp) → *Metric:* iterations & geometric mean runtime on Netlib LP Collection.
- *Finding:* Normal equations square the condition number → *PS requirement:* R9, R13 → *Component:* src/linalg/dense_lu.cpp / proposed sparse LDL^T path → *Metric:* [[KKT Residual]] drift and certified status rate (exit codes 0/7 in STATUS.md).

## Related Papers
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Ye-1998-Crossover-Interior-Point]]

## Uses
- [[Interior-Point Method]]
- [[Sparse LDL Factorization]]
- [[KKT Residual]]
- [[Ill-Conditioning]]
