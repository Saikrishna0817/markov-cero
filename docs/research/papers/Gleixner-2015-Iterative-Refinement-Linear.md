---
type: paper
title: "Iterative Refinement for Linear Programming"
authors: "Gleixner, Steffy & Wolter"
year: 2015
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Iterative Refinement for Linear Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> LP-specific refinement that yields certified (even exact) solutions in SoPlex — the blueprint for turning a floating-point LP solver into a certifying one.

## Metadata
| Field | Value |
|---|---|
| Authors | Gleixner, Steffy & Wolter |
| Year | 2015 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified; Optimization Online PDF in list) |

## Problem Addressed
Ordinary refinement assumes you only want a more accurate vector; LP additionally needs a *basis* that is genuinely optimal and a primal solution that is genuinely feasible — tolerances otherwise let a solver declare optimality on a false basis, especially on degenerate or ill-conditioned instances.

## Core Contribution
- **Methodology:** Refine the primal and dual solutions jointly, re-verify optimality/feasibility exactly (or in increasing precision), and iterate until a certified optimum or infeasibility/unboundedness certificate is obtained; supports extended- and arbitrary-precision fallbacks.
- **Assumptions:** Ability to evaluate residuals exactly or at controlled precision; a certifiable LP formulation (boundedness handling).
- **Benchmarks/datasets:** Hard/degenerate LPs where standard tolerances fail.
- **Metrics:** Certified vs. uncertified solves; precision required; time overhead.
- **Key results:** Achieves certified solutions (SoPlex) with modest overhead — the strongest literature-based path to R17's "demonstrate numerical robustness".

## Engineering-Relevant Knowledge
**Algorithms:** Mixed-precision LP refinement; exact/verified certification of primal-dual optimality.
**Techniques:** Adaptive precision escalation (double → extended → exact), exact residual evaluation, dual feasibility re-check after each refinement.
**Implementation details:** We already have a zero-trust KKT verifier (`src/verify/primal_verifier.cpp`) and a reference LP verifier — refinement *in the solver* would connect them: verifier drives refinement until it passes. This is the single most aligned numerics component with our architecture.
**Equations/rules:** Certified when exact arithmetic shows c − Aᵀy ≥ 0, y ≥ 0, cᵀx = bᵀy, Ax = b, x ≥ 0 (no ε slack); otherwise refine.
**Limitations/failure cases:** Overhead grows on very ill-conditioned instances; exact fallback can be slow; needs careful handling of unbounded/infeasible LPs.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Named in the priority reading order for the project's differentiator; combines with our existing verifiers to produce the R17 robustness evidence and the certificate part of R17/R18.

## Evidence → Engineering Decision
- *Finding:* verifiers exist but the solver does not refine toward certification → *PS requirement:* R9, R17 → *Component:* src/lp/dual/dual_simplex.cpp, src/verify/primal_verifier.cpp → *Metric:* certified-solve rate, [[KKT Residual]]

## Related Papers
- [[Moler-1967-Rounding-Errors-Algebraic]]
- [[Cline-1979-Estimate-Condition-Number]]
- [[Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Unknown-2026-Verified-Linear-Programming]]

## Uses
- [[Iterative Refinement]] [[KKT Residual]] [[Numerical Stability]] [[Ill-Conditioning]]
