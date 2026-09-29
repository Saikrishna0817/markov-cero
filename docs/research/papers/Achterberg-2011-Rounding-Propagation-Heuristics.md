---
type: paper
title: "Rounding and Propagation Heuristics for Mixed Integer Programming"
authors: "Achterberg, Berthold & Hendel"
year: 2011
venue: "ZIB"
doi: "(unverified)"
domain: [heuristics]
priority: ✦
status: standard
tags: [paper, heuristics]
---

# Rounding and Propagation Heuristics for Mixed Integer Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Focused study of the cheapest heuristic tier: when does plain rounding work, and how much does constraint propagation add?

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg, Berthold & Hendel |
| Year | 2011 |
| Venue | ZIB Report (list: "ZIB"; ZR-11-29 PDF) |
| DOI/URL | (unverified; ZIB PDF in list) |

## Problem Addressed
Rounding is nearly free but often infeasible; propagation-based repair adds cost. The paper measures the crossover — which is exactly the decision our simple rounding heuristic has to make.

## Core Contribution
- **Methodology:** Implement rounding heuristics (round LP solution, check, optionally repair) with and without constraint propagation; compare success rate and cost across instance classes.
- **Assumptions:** LP solution at a node; propagator capable of inferring bounds/implications.
- **Benchmarks/datasets:** MIPLIB-class instances.
- **Metrics:** Success rate, time per call, primal integral contribution.
- **Key results:** Propagation substantially raises rounding success for modest cost; gives the parameters for a "try rounding first, propagate only if needed" policy.

## Engineering-Relevant Knowledge
**Algorithms:** LP rounding with feasibility check; propagation-based repair/implication search.
**Techniques:** Lazy escalation (cheap rounding first); tolerance-consistent feasibility tests.
**Implementation details:** `src/milp/heuristics.cpp` does rounding; we have no general propagator (presolve has 4 rules). A limited row-based propagation (bound tightening from single rows) is a feasible subset.
**Equations/rules:** Round x̄_j to nearest integer within [l_j, u_j]; accept if Σ a_ij x_j satisfies row i within feasibility tolerance for all i.
**Limitations/failure cases:** Without propagation, success collapses on tightly coupled models; propagation must not be applied with tighter-than-LP tolerances (produces false "infeasible").

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the tuning guide for the heuristic we already have, and it quantifies the value of the missing propagation step (R5).

## Evidence → Engineering Decision
- *Finding:* plain rounding success is limited without propagation → *PS requirement:* R5 → *Component:* src/milp/heuristics.cpp, src/presolve/presolve.cpp → *Metric:* rounding success rate, primal integral

## Related Papers
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Hillier-1969-Efficient-Heuristic-Procedures]]
- [[Berthold-2006-Primal-Heuristics-Mixed]]
- [[Berthold-2023-Feasibility-Jump]]

## Uses
- [[Rounding Heuristic]] [[LP Relaxation]] [[Diving]] [[KKT Residual]]
