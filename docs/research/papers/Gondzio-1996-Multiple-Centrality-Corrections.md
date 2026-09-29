---
type: paper
title: "Multiple Centrality Corrections in a Primal-Dual Method"
authors: "Gondzio"
year: 1996
venue: "CPAA"
doi: "(unverified)"
domain: [ipm]
priority: ○
status: standard
tags: [paper, ipm]
---
# Multiple Centrality Corrections in a Primal-Dual Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Repeated centrality corrections restore iterates to the central path after aggressive steps — robustness at low extra cost.
## Metadata
| Field | Value |
|---|---|
| Authors | Gondzio |
| Year | 1996 |
| Venue | CPAA (Computational Optimization and Applications) |
| DOI/URL | (unverified) |
## Problem Addressed
Aggressive predictor steps (Mehrotra style) often land iterates far from the central path, forcing tiny subsequent steps or outright failure on hard instances. One corrector is sometimes not enough, especially near degeneracy or when the system is ill-conditioned.
## Core Contribution
- **Methodology:** After the (affine) predictor, apply a *sequence* of centrality corrections, each moving the iterate back toward the neighborhood without changing the primal-dual affine direction; stop when centrality is acceptable.
- **Assumptions:** Same as Mehrotra; corrections are cheap because they reuse the same factorization (multiple RHS updates).
- **Benchmarks/datasets:** Difficult LPs where single-corrector methods stall (paper's test set).
- **Metrics:** Steps taken, iterations, failures recovered.
- **Key results:** Better robustness on troublesome instances with negligible extra factorizations (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Predictor-corrector with multiple right-hand-side corrections.
**Techniques:** Factorization reuse across correction steps; centrality measure as acceptance test.
**Implementation details:** Requires storing the factorization for one iteration and applying it k times — interacts with refactorization policy in a future `src/lp/ipm/`.
**Limitations/failure cases:** Extra triangular solves per iteration; gains vanish if linear solves are inaccurate.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R9/R17 emphasize reliable convergence on degenerate, ill-conditioned models; this is the cheapest known stabilizer for an IPM (not yet present in markov-cero).
## Evidence → Engineering Decision
- *Finding:* Extra centrality corrections cost triangular solves, not factorizations → *PS requirement:* R9, R12 → *Component:* src/lp/ipm/ (proposed) → *Metric:* [[KKT Residual]] at termination vs. solve time.
## Related Papers
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]
## Uses
- [[Interior-Point Method]]
