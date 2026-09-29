---
type: paper
title: "A Primal-Dual Interior-Point Algorithm for Linear Programming"
authors: "Kojima, Mizuno & Yoshise"
year: 1989
venue: "(not listed in source)"
doi: "(unverified)"
domain: [ipm]
priority: ★
status: deep
tags: [paper, ipm]
---

# A Primal-Dual Interior-Point Algorithm for Linear Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Path-following primal-dual framework with polynomial complexity — the direct ancestor of every modern LP interior-point solver.

## Metadata
| Field | Value |
|---|---|
| Authors | Kojima, Mizuno & Yoshise |
| Year | 1989 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |

## Problem Addressed
Karmarkar-type methods were tied to projective transformations and awkward to generalize to inequalities, bounds and dual information. A symmetric primal-dual formulation was needed that works on the standard-form LP directly and connects to duality theory. The paper supplied that framework plus convergence proofs.

## Core Contribution
- **Methodology:** Follow the central path defined by `A x = b, x > 0`, `A^T y + s = c`, `x_i s_i = mu`; predictor/corrector style steps keep the iterate in a neighborhood of the path while `mu -> 0`; polynomiality from neighborhood invariance.
- **Assumptions:** Strictly feasible start (or an elastic/artificial start variant), positive iterates, exact (to tolerance) solution of the Newton system each iteration.
- **Benchmarks/datasets:** Netlib-class LPs in the later computational literature (this paper is primarily theoretical).
- **Metrics:** Iteration complexity in `sqrt(n) log(1/epsilon)`; neighborhood widths.
- **Key results:** Polynomial convergence of primal-dual path-following; O(sqrt(n) log(1/epsilon)) iteration class for short-step variants (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Primal-dual path-following; central-path Newton steps.
**Techniques:** Neighborhood tracking; step-length ratio tests to keep positivity; complementarity measure `x^T s`.
**Implementation details:** One symmetric linear system per iteration (normal equations `A Theta A^T` or augmented saddle system); markov-cero would reuse `src/linalg/` sparse kernels and [[Sparse LDL Factorization]].
**Equations/rules:** centrality `x_i s_i >= sigma * mu`; step length = min of primal/dual positivity limits.
**Limitations/failure cases:** Needs a feasible (or heavily perturbed) start; pure path-following needs many iterations — practical codes move to Mehrotra predictor-corrector.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R4 names interior-point methods; the PS also stresses numerical robustness (R9, R13). This paper's central-path/neighbourhood machinery is the theoretical contract any IPM in markov-cero must satisfy — absent today (engine list: simplex, dual simplex, PDLP, MILP, QP).

## Evidence → Engineering Decision
- *Finding:* Newton system must be solved to tolerance each iteration or the neighborhood invariant breaks → *PS requirement:* R9, R17 → *Component:* src/linalg/sparse_basis.cpp (factor/solve kernels to reuse) → *Metric:* [[KKT Residual]] / [[Primal Residual]] per iteration.
- *Finding:* Starting-point feasibility is a real design burden → *PS requirement:* R9 → *Component:* src/presolve/presolve.cpp (presolve reduces infeasibility before solve) → *Metric:* presolve reduction rate, solve failure rate.

## Related Papers
- [[Karmarkar-1984-New-Polynomial-Time-Algorithm]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]

## Uses
- [[Interior-Point Method]]
- [[KKT Conditions]]
- [[Duality Gap]]
