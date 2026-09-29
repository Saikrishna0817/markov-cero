---
type: paper
title: "Strong Formulations for Mixed Integer Programming: A Survey"
authors: "Wolsey"
year: 1989
venue: "(not listed in source)"
doi: "(unverified)"
domain: [milp]
priority: ○
status: standard
tags: [paper, milp]
---
# Strong Formulations for Mixed Integer Programming: A Survey

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Formulation strength determines tree size before any algorithm touches the model.
## Metadata
| Field | Value |
|---|---|
| Authors | Wolsey |
| Year | 1989 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |
## Problem Addressed
Two formulations of the same integer problem can have wildly different LP relaxation quality; no amount of search fixes a weak relaxation. The survey systematizes how to construct formulations whose integer hull is better approximated.
## Core Contribution
- **Methodology:** Techniques for tightening: aggregation, disaggregation, covering/packing formulations, lifting, variable redefinition; measures of formulation strength.
- **Assumptions:** Finite MILP; comparison by LP relaxation bounds.
- **Benchmarks/datasets:** Expository examples (lot-sizing, network).
- **Metrics:** Root relaxation bound; gap closed by formulation alone.
- **Key results:** Better formulation >> better search on many models (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Formulation strengthening (pre-solve design time).
**Techniques:** Lifting; aggregation; covering reformulations.
**Implementation details:** As a solver, markov-cero receives whatever formulation the user passes (`src/io/mps.cpp`) — but probing/aggregation inside presolve (Savelsbergh, Achterberg 2020) effectively performs formulation strengthening at runtime.
**Limitations/failure cases:** Model-specific; not automatable in general; solver-side substitutes are heuristics.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R11 (industrial models arrive fixed) + R13 (weak relaxations): explains why our presolve must compensate for user formulations and how to report root gap honestly.
## Evidence → Engineering Decision
- *Finding:* Root gap quality is a formulation property the solver can only partially improve → *PS requirement:* R13, R20 → *Component:* src/presolve/presolve.cpp → *Metric:* root [[Relative Optimality Gap]] reported in telemetry.
## Related Papers
- [[Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Land-1960-Automatic-Method-Solving]]
## Uses
- [[LP Relaxation]]
- [[Weak Relaxation]]
