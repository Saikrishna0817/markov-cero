---
type: paper
title: "An Implementation of a Primal-Dual Interior Point Method for Linear Programming"
authors: "McShane, Monma & Shanno"
year: 1989
venue: "IJOC"
doi: "10.1287/ijoc.1.1.70 (derived from list link)"
domain: [ipm]
priority: ✦
status: standard
tags: [paper, ipm]
---
# An Implementation of a Primal-Dual Interior Point Method for Linear Programming
> Early full implementation report of primal-dual IPM (pre-Mehrotra): initialization, tolerances and sparse linear algebra lessons.
## Metadata
| Field | Value |
|---|---|
| Authors | McShane, Monma & Shanno |
| Year | 1989 |
| Venue | IJOC |
| DOI/URL | 10.1287/ijoc.1.1.70 |
## Problem Addressed
Primal-dual IPM theory existed (Kojima et al.), but nobody had documented what it takes to run it end-to-end on real LP data: starting points, tolerances, refactorization, termination. The paper is the practical companion to the theory.
## Core Contribution
- **Methodology:** Working primal-dual IPM implementation with explicit initialization (including infeasible starts), step-length heuristics, sparse factorization of the Newton system.
- **Assumptions:** Sparse LP; Cholesky-class SPD systems (normal equations era); tolerance framework for feasibility/optimality.
- **Benchmarks/datasets:** Netlib-class LPs of the late 1980s.
- **Metrics:** Iterations, time, accuracy vs. simplex baselines.
- **Key results:** Competitive accuracy on medium LPs; documented sensitivity to scaling and tolerances (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Primal-dual path-following IPM, production-style.
**Techniques:** Infeasible-start handling; presolve-lite cleanup; refactorization scheduling.
**Implementation details:** Confirms that [[Scaling]] and tolerance choice matter as much as the algorithm — matches our Ruiz scaling stage (`src/scale/ruiz_scaling.cpp`) as prerequisite for any new engine.
**Limitations/failure cases:** Pre-predictor-corrector iteration counts; normal-equation conditioning limits accuracy.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Historical implementation checklist for R4; less actionable than Mehrotra/Wright but good for estimating engineering effort of an IPM (tolerances, refactorization policy, termination).
## Evidence → Engineering Decision
- *Finding:* Tolerances and scaling decide practical IPM accuracy → *PS requirement:* R9 → *Component:* src/scale/ruiz_scaling.cpp + CLI `--tolerance` → *Metric:* [[KKT Residual]] vs. requested tolerance.
## Related Papers
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
## Uses
- [[Interior-Point Method]]
