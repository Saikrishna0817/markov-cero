---
type: paper
title: "Pivot Selection Methods of the Devex Simplex Code"
authors: "Harris"
year: 1973
venue: "Math. Prog."
doi: "(unverified)"
domain: [lp, numerics]
priority: ★
status: deep
tags: [paper, lp, numerics]
---

# Pivot Selection Methods of the Devex Simplex Code

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Tolerance-based (Harris) ratio test plus Devex pricing — the defaults buried inside every modern simplex.

## Metadata
| Field | Value |
|---|---|
| Authors | Harris |
| Year | 1973 |
| Venue | Math. Prog. |
| DOI/URL | (unverified) |

## Problem Addressed
The ratio test written for exact arithmetic fails in floating point: tiny rounding differences make it pick arbitrary pivots among near-ties, causing loss of accuracy, loss of sparsity and stalled progress on degenerate LPs. The paper studies pivot selection as implemented in the Devex code and introduces the tolerance-based ratio test now standard everywhere.

## Core Contribution
- **Methodology:** Empirical analysis of pivot selection (entering-column rule and leaving-row ratio test) in a working code, replacing the exact-minimum ratio test with a Harris tolerance rule that accepts any row within a tolerance band of the minimum and then maximizes the pivot among them.
- **Assumptions:** A small multiplicative tolerance is safe relative to data scale; near-ties are the norm on degenerate models (approximate).
- **Benchmarks/datasets:** Contemporary LP test problems through the Devex code (qualitative; no figures asserted).
- **Metrics:** Iterations, solution accuracy, sparsity of updates (qualitative).
- **Key results:** Tolerance-band ratio testing plus devex (approximate unit-length) pricing makes the simplex markedly more robust on degenerate problems (qualitative/approximate).

## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex ratio test; Devex pricing as a cheap steepest-edge approximation.

**Techniques:** Harris Ratio Test (band of relative tolerance around the minimum ratio), maximizing |pivot| within the band to limit rounding, Devex weight updates.

**Implementation details:** Directly implementable in src/lp/reference/revised_simplex.cpp and src/lp/dual/dual_simplex.cpp: gather ratios, find min, accept all rows ≤ (1+τ)·min, choose the largest-magnitude pivot among them; τ must be a documented, tested constant (R17 evidence).

**Equations/rules:** Accept candidate rows i with ratioᵢ ≤ (1+τ)·minⱼ ratioⱼ, then select max |ȳᵢ| among them (approximate formulation of the Harris rule).

**Limitations/failure cases:** τ too large accepts real infeasibility (must be caught by verification); τ too small degenerates to the exact-arithmetic rule; interacts with scaling — tolerance is meaningless if row magnitudes are wildly different (Scaling).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R13 (degeneracy) and R9 (numerical stability) meet in the ratio test — the single most safety-critical routine in simplex; a from-scratch solver that omits Harris tolerances will misbehave on exactly the hard models R17 asks us to demonstrate robustness on.

## Evidence → Engineering Decision
- *Finding:* Exact-arithmetic ratio tests misbehave in floating point on near-ties → *PS requirement:* R13 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Numerical Error
- *Finding:* Tolerance band must be a tuned, documented constant, not a magic literal → *PS requirement:* R17 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* KKT Residual

## Related Papers
- [[Koberstein-2005-Dual-Simplex-Method]]
- [[Goldfarb-1977-Practicable-Steepest-Edge]]
- [[Gill-1989-Practical-Anti-Cycling]]
- [[Bland-1977-Anti-Cycling-Rule]]

## Uses
- [[Harris Ratio Test]] [[Degeneracy]] [[Numerical Stability]]
