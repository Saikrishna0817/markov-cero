---
type: paper
title: "A New Polynomial-Time Algorithm for Linear Programming"
authors: "Karmarkar"
year: 1984
venue: "Combinatorica"
doi: "10.1007/BF02579150 (derived from list link)"
domain: [ipm]
priority: ★
status: deep
tags: [paper, ipm]
---

# A New Polynomial-Time Algorithm for Linear Programming

> Origin of interior-point methods: projective-scaling algorithm polynomial in theory and (claimed) faster than simplex in practice.

## Metadata
| Field | Value |
|---|---|
| Authors | Karmarkar |
| Year | 1984 |
| Venue | Combinatorica |
| DOI/URL | 10.1007/BF02579150 |

## Problem Addressed
The ellipsoid method was polynomial but hopelessly slow, while simplex remained exponential in the worst case. Karmarkar wanted an algorithm with both a polynomial bound and practical speed on large sparse LPs. The paper launched the entire interior-point research program.

## Core Contribution
- **Methodology:** Projective transformation maps the feasible polytope onto the simplex; take gradient steps that keep iterates in the interior (centered), rescale after each step; complexity proved in the bit model independent of constraint size after normalization.
- **Assumptions:** Feasible starting point (original used a big-M embedding); problem scaled so the start is near the polytope center; equality form `A x = 0` normalization.
- **Benchmarks/datasets:** Karmarkar's AT&T test LPs (a few hundred variables; originally proprietary).
- **Metrics:** Iteration count, bit-complexity, wall-clock vs. simplex (1984-era code).
- **Key results:** Polynomial bound often cited as ~O(n^3.5 L) class (approximate); reported ~50x speedup vs. simplex on his tests (approximate, hardware-era dependent).

## Engineering-Relevant Knowledge
**Algorithms:** Projective/affine-scaling interior-point method; centering neighborhood.
**Techniques:** Scaling to centered start; interior feasibility maintenance; projected gradient steps.
**Implementation details:** Original dense normal equations `A A^T`; modern ports instead solve sparse augmented systems (see [[Sparse LDL Factorization]]) and require COLAMD-style orderings.
**Equations/rules:** iterate must stay strictly positive in transformed coordinates; step length limited so no coordinate hits the boundary.
**Limitations/failure cases:** big-M start infeasible in practice; sensitive to [[Scaling]]; competitive only after Mehrotra-style predictor-corrector + sparse linear algebra.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R4 requires an interior-point engine alongside simplex; markov-cero currently has PDLP (first-order) only — no IPM code. Karmarkar's centering/scaling ideas feed the initialization and [[Scaling]] stages of a future `src/lp/ipm/` module, not today's PDLP.

## Evidence → Engineering Decision
- *Finding:* Interior feasibility + centering depend on a well-scaled start → *PS requirement:* R4, R9 → *Component:* src/scale/ruiz_scaling.cpp (prerequisite scaling before any IPM) → *Metric:* [[KKT Residual]] at termination.
- *Finding:* Projective method's dense linear algebra does not scale → *PS requirement:* R6, R12 → *Component:* src/lp/first_order/pdlp.cpp (first-order route chosen until sparse IPM exists) → *Metric:* geometric mean runtime on Netlib LP Collection.

## Related Papers
- [[Khachiyan-1979-Polynomial-Algorithm-Linear]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]

## Uses
- [[Interior-Point Method]]
- [[Scaling]]
- [[KKT Residual]]
