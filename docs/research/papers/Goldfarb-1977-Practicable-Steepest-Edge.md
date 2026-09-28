---
type: paper
title: "A Practicable Steepest-Edge Simplex Algorithm"
authors: "Goldfarb & Reid"
year: 1977
venue: "Math. Prog."
doi: "(unverified)"
domain: [lp]
priority: ★
status: deep
tags: [paper, lp]
---

# A Practicable Steepest-Edge Simplex Algorithm

> Makes steepest-edge pricing affordable: efficient weight updates turn a theoretical ideal into a working pricing rule.

## Metadata
| Field | Value |
|---|---|
| Authors | Goldfarb & Reid |
| Year | 1977 |
| Venue | Math. Prog. |
| DOI/URL | (unverified) |

## Problem Addressed
Steepest-edge pricing — choosing the entering column with the largest reduced cost per unit of geometric step — was known theoretically to reduce iteration counts dramatically, but was assumed impractical because edge weights had to be recomputed or maintained at prohibitive cost per pivot. The paper provides the update formulas and bookkeeping that make it run.

## Core Contribution
- **Methodology:** Derives economical formulas for updating all steepest-edge weights after each pivot (using the leaving column and the ratio-test quantities), plus a restart/recomputation strategy when weights drift.
- **Assumptions:** Floating-point weights approximate true edge lengths and degrade gradually; recomputation can be scheduled (approximate).
- **Benchmarks/datasets:** LPs where naive pricing iterates heavily (qualitative; no instance list asserted).
- **Metrics:** Iterations to optimum vs. weight-maintenance overhead (qualitative).
- **Key results:** Steepest edge becomes computationally practical — iteration reductions large enough to outweigh per-pivot weight updates (approximate; no figures asserted).

## Engineering-Relevant Knowledge
**Algorithms:** Steepest-edge pricing in the revised simplex (primal form here; dual form follows in Goldfarb & Forrest).

**Techniques:** Edge-weight rank-one updates, periodic weight refresh, fallback pricing (Devex/first-of-row) when weights are suspect.

**Implementation details:** For src/lp/reference/revised_simplex.cpp and src/lp/dual/dual_simplex.cpp: instrument both the extra flops of weight maintenance and the iteration savings per model class — the paper's claim is empirical, and ours must be too (R16 methodology).

**Equations/rules:** Pivot score = |c̄ⱼ| / ‖dⱼ‖ where dⱼ is the edge direction; after pivot, weights update via one correction term using ȳ (the ratio-test column) (approximate).

**Limitations/failure cases:** Weight drift under many updates degrades pricing toward devex quality without warning; overhead can dominate on easy LPs (approximate).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R13's degenerate/hard models are precisely where first-of-row pricing explodes in iterations; steepest edge is our primary lever against that, and its cost/benefit is directly measurable against the Devex baseline (R4, R16, R20).

## Evidence → Engineering Decision
- *Finding:* Geometric pricing cuts iterations on hard models when weight updates are cheap → *PS requirement:* R13 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Weight drift needs scheduled refresh to keep pricing valid → *PS requirement:* R9 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* Numerical Error

## Related Papers
- [[Goldfarb-1992-Steepest-Edge-Simplex]]
- [[Harris-1973-Pivot-Selection-Methods]]
- [[Hall-2005-Hyper-sparsity-Revised-Simplex]]
- [[Koberstein-2005-Dual-Simplex-Method]]

## Uses
- [[Steepest Edge]] [[Reduced Cost]] [[Revised Simplex]]
