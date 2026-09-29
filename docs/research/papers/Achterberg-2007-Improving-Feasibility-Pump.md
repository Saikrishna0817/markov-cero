---
type: paper
title: "Improving the Feasibility Pump"
authors: "Achterberg & Berthold"
year: 2007
venue: "Discrete Optim."
doi: "(unverified)"
domain: [heuristics]
priority: ★
status: deep
tags: [paper, heuristics]
---

# Improving the Feasibility Pump

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Objective Feasibility Pump: bias the projection with the LP objective, cutting the reported mean gap from 55% to 29.5%.

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg & Berthold |
| Year | 2007 |
| Venue | Discrete Optimization (list: "Discrete Optim.") |
| DOI/URL | (unverified; ZIB report ZR-05-42 PDF in list) |

## Problem Addressed
The classic FP (#110/#111) optimizes feasibility only: it can pump out a feasible point that is arbitrarily bad in objective value, so bounds barely tighten and branch-and-bound still has to do all the work.

## Core Contribution
- **Methodology:** Add the objective to the projection step as a weighted secondary term, so rounding and re-projection are pulled toward good objective values; also add better infeasibility measures and restart strategies.
- **Assumptions:** LP relaxation provides an objective reference; weight between feasibility and objective terms needs tuning.
- **Benchmarks/datasets:** Standard MIP benchmarks; list records "mean gap 55% → 29.5%".
- **Metrics:** Objective gap of the FP solution at termination (mean relative gap); success rate.
- **Key results:** Mean gap 55% → 29.5% — a large, easily measured quality improvement over the classic pump, and a cheap alternative to running a full local search.

## Engineering-Relevant Knowledge
**Algorithms:** [[Feasibility Pump]] with objective term (OFPT); infeasibility measurement for choosing the rounding direction.
**Techniques:** Weighted projection min (‖x − x*‖ + α·|obj(x) − obj_LP|); tie-breaking by objective when several roundings are equally close; restarts.
**Implementation details:** Direct upgrade to `src/milp/heuristics.cpp`: replace pure feasibility projection with the weighted form; α tuning should be reported in evidence, since the 55%→29.5% figure is our quality target for FP outputs.
**Equations/rules:** projection objective = distance term + α × objective-deviation term; choose α so the objective term dominates only when feasibility is unchanged.
**Limitations/failure cases:** α too large ⇒ infeasible rounding loops; still no guarantee of improvement and each iteration costs an LP solve.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** We ship the pump already; this is the cheapest available upgrade with a published, citable effect size (gap 55%→29.5%) — ideal for the R16 comparison narrative.

## Evidence → Engineering Decision
- *Finding:* objective-blind FP yields poor incumbents → *PS requirement:* R5, R16, R20 → *Component:* src/milp/heuristics.cpp → *Metric:* mean FP gap, primal integral, [[Relative Optimality Gap]]

## Related Papers
- [[Fischetti-2005-Feasibility-Pump]]
- [[Fischetti-2006-Feasibility-Pump-Heuristic]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Achterberg-0000-Objective-Feasibility-Pump]]

## Uses
- [[Feasibility Pump]] [[Rounding Heuristic]] [[Relative Optimality Gap]] [[LP Relaxation]]
