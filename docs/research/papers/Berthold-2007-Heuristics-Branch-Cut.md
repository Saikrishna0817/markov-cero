---
type: paper
title: "Heuristics of the Branch-Cut-and-Price-Framework SCIP"
authors: "Berthold"
year: 2007
venue: "ZIB"
doi: "(unverified)"
domain: [heuristics]
priority: ★
status: deep
tags: [paper, heuristics]
---

# Heuristics of the Branch-Cut-and-Price-Framework SCIP

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The reference taxonomy of 23 primal heuristics (rounding, 6 diving variants, objective diving, LNS family) with a schedule for when to run each.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold |
| Year | 2007 |
| Venue | ZIB Report (list: "ZIB"; ZR-07-30 PDF) |
| DOI/URL | (unverified; ZIB report PDF in list) |

## Problem Addressed
MIP solvers accumulate heuristics ad hoc: nobody states which heuristic to call, at which node, with what frequency, and at what cost — so implementations over- or under-invest in each.

## Core Contribution
- **Methodology:** Classify heuristics by *how* they search (trivial rounding, LP rounding, diving, objective diving, local search/LNS: RENS, RINS, local branching, crossover), report computational contribution of each, and prescribe an execution schedule with frequency limits.
- **Assumptions:** Working branch-and-cut with LP at nodes; incumbent handling and randomization support.
- **Benchmarks/datasets:** Standard MIP benchmark set (SCIP-era).
- **Metrics:** Incumbents found, primal integral, time share per heuristic.
- **Key results:** Single best map of the heuristic design space — tells an implementer exactly what is missing after "rounding + feasibility pump".

## Engineering-Relevant Knowledge
**Algorithms:** Rounding, [[Diving]], objective diving, RENS, RINS, local branching, crossover — plus frequency/duty scheduling.
**Techniques:** Duty/frequency control (how often to try each heuristic), randomization seeds, early abort on failure, incumbent update hooks.
**Implementation details:** `src/milp/heuristics.cpp` currently implements only rounding + FP ⇒ per this taxonomy we cover 2 of ~6 cheap tiers and none of the LNS tier; adding diving is the cheapest next step (fix one variable per LP, iterate).
**Equations/rules:** Scheduling rule of thumb: trivial/rounding heuristics at every node (cheap), FP at root, LNS periodically with a time budget.
**Limitations/failure cases:** Each LNS invocation competes with tree search; heavy schedules can waste 10–20% of runtime on futile heuristics.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the checklist against which our heuristic inventory is measured (audit: "needs dive/RINS") and the source of the scheduling policy we should implement next.

## Evidence → Engineering Decision
- *Finding:* 2 heuristics shipped vs. a taxonomy of 23 → *PS requirement:* R5, R20 → *Component:* src/milp/heuristics.cpp → *Metric:* primal integral, incumbents per solve

## Related Papers
- [[Berthold-2006-Primal-Heuristics-Mixed]]
- [[Danna-2004-Exploring-Relaxation-Induced]]
- [[Achterberg-2007-Improving-Feasibility-Pump]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics]]

## Uses
- [[Diving]] [[Feasibility Pump]] [[Rounding Heuristic]] [[Branch and Cut]]
