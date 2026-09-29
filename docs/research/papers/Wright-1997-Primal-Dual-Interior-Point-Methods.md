---
type: paper
title: "Primal-Dual Interior-Point Methods"
authors: "Wright"
year: 1997
venue: "SIAM"
doi: "10.1137/1.9781611971453"
domain: [ipm]
priority: ★
status: deep
tags: [paper, ipm]
---

# Primal-Dual Interior-Point Methods

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Book-length specification of LP interior-point methods: algorithm, sparse linear algebra, tolerances and stopping criteria.

## Metadata
| Field | Value |
|---|---|
| Authors | Wright |
| Year | 1997 |
| Venue | SIAM |
| DOI/URL | 10.1137/1.9781611971453 |

## Problem Addressed
Interior-point research was scattered across journal papers with inconsistent notation and no complete implementation recipe. A from-scratch implementer has no single reference covering initialization, step rules, linear algebra, error tolerances and termination proofs together. This book is that reference.

## Core Contribution
- **Methodology:** Full derivation of primal-dual IPM for LP: central path, predictor-corrector variants (including complete Mehrotra specification), infeasibility detection, stopping criteria tied to duality/complementarity.
- **Assumptions:** Exact-ish solution of each Newton system; floating-point model with stated tolerance framework; sparse linear algebra with reordering.
- **Benchmarks/datasets:** Netlib examples used illustratively across chapters.
- **Metrics:** Duality gap, infeasibility norms, iteration counts; tolerance selection rules.
- **Key results:** Complete reproducible algorithm boxes; proofs that predictor-corrector inherits polynomiality; practical guidance on tolerance setting (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Mehrotra predictor-corrector; path-following variants; infeasibility/unboundedness detection via residuals.
**Techniques:** Sparse normal equations vs. augmented systems; symmetric indefinite LDL^T; stopping tests scaled by `max(1, |c^T x|)`.
**Implementation details:** Chapter-level detail on scaling before factorization, iterative refinement of Newton solves, and crossover hand-off to simplex — exactly the pieces markov-cero lacks (`src/lp/ipm/` does not exist; `src/lp/first_order/pdlp.cpp` is the only non-simplex LP engine).
**Equations/rules:** stopping when primal residual, dual residual and gap are each below `tol * scale`; `mu` update rule and step safeguard given explicitly.
**Limitations/failure cases:** Floating-point IPM loses accuracy on badly scaled/degenerate LPs without refinement; no basis is produced — requires [[Crossover]] for a vertex solution or a warm start.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R4 requires interior-point methods and R9/R17 numerical robustness; this book is the build manual. It is also the bridge to the QP requirement (R2) since the same machinery extends to convex QP (see [[Wright-1997-Primal-Dual-IPM]]).

## Evidence → Engineering Decision
- *Finding:* IPM termination must be a tolerance contract (gap, primal/dual residuals) → *PS requirement:* R9, R17 → *Component:* src/verify/reference_lp_verifier.cpp (verifier reused as IPM stopping oracle) → *Metric:* [[KKT Residual]] vs. certified exit code (STATUS.md exit 0/7).
- *Finding:* No basis from IPM → *PS requirement:* R4 (IPM + revised simplex together) → *Component:* src/lp/reference/revised_simplex.cpp (crossover target) → *Metric:* crossover simplex iterations to optimality.

## Related Papers
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Lustig-1994-Interior-Point-Methods]]
- [[Ye-1998-Crossover-Interior-Point]]
- [[Wright-1997-Primal-Dual-IPM]]

## Uses
- [[Interior-Point Method]]
- [[KKT Conditions]]
- [[Sparse LDL Factorization]]
- [[Crossover]]
