---
type: paper
title: "Steepest Edge Simplex Algorithms for Linear Programming"
authors: "Goldfarb & Forrest"
year: 1992
venue: "Math. Prog."
doi: "(unverified)"
domain: [lp]
priority: ★
status: deep
tags: [paper, lp]
---

# Steepest Edge Simplex Algorithms for Linear Programming

> Dual steepest-edge pricing — the scheme that keeps degenerate and near-degenerate LP relaxations moving.

## Metadata
| Field | Value |
|---|---|
| Authors | Goldfarb & Forrest |
| Year | 1992 |
| Venue | Math. Prog. |
| DOI/URL | (unverified) |

## Problem Addressed
Simplex pricing rules based on reduced cost alone (first-of-row, Devex) behave erratically on degenerate LPs: they take many tiny or repeated pivots because they do not measure the true geometric length of an edge. Steepest-edge pricing normalizes each candidate by its edge length, and the paper develops it — especially in its dual form — for exactly the models where other pricing collapses.

## Core Contribution
- **Methodology:** Derives primal and dual steepest-edge pricing with efficient formulas for updating edge weights as the basis changes, plus strategies for keeping the weights accurate over long runs.
- **Assumptions:** Weights are maintained in floating point and drift, so updates/resets are required; reduced costs remain the optimality signal (approximate).
- **Benchmarks/datasets:** Netlib-class LPs including degenerate ones (approximate; no instance list asserted).
- **Metrics:** Iterations to optimum, time per iteration, quality on degenerate models (qualitative).
- **Key results:** Steepest-edge pricing dramatically cuts iteration counts on hard and degenerate LPs, and the dual variant is what production MIP node solves rely on (approximate; no figures asserted).

## Engineering-Relevant Knowledge
**Algorithms:** Dual steepest-edge pricing for the dual simplex; primal steepest-edge for the revised simplex.

**Techniques:** Normalized edge-length pricing, weight update along the chosen edge, Devex/fallback pricing when weights drift, periodic weight resets.

**Implementation details:** In src/lp/dual/dual_simplex.cpp and src/lp/reference/revised_simplex.cpp the trade is measurable: extra flops per pivot vs. fewer pivots — with hyper-sparse solves the weight update is often cheaper than the iteration it saves (Hall & McKinnon is the companion measurement).

**Equations/rules:** Pivot score = |c̄ⱼ| / ‖eⱼᵀB⁻¹‖ (reduced cost divided by edge length); after pivoting, weights update by a rank-one formula using the leaving column (approximate).

**Limitations/failure cases:** Weight drift silently degrades pricing toward random pricing — needs monitoring/reset; on easy LPs the overhead may not pay (approximate).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R13 names highly degenerate models explicitly; degenerate MIP relaxations are where devex pricing stalls, so dual steepest edge is the primary defense — and its cost/benefit is directly measurable against our current pricing (R16/R20).

## Evidence → Engineering Decision
- *Finding:* Degenerate LPs need geometric pricing, not raw reduced cost → *PS requirement:* R13 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Weight maintenance must be monitored or pricing degrades silently → *PS requirement:* R9 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* KKT Residual

## Related Papers
- [[Koberstein-2005-Dual-Simplex-Method]]
- [[Goldfarb-1977-Practicable-Steepest-Edge]]
- [[Harris-1973-Pivot-Selection-Methods]]
- [[Hall-2005-Hyper-sparsity-Revised-Simplex]]

## Uses
- [[Steepest Edge]] [[Reduced Cost]] [[Degeneracy]]
