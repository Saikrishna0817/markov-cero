---
type: paper
title: "Primal Heuristics for Mixed-Integer Nonlinear Programming"
authors: "Berthold"
year: 2025
venue: "(chapter; Cambridge link in list)"
doi: "(unverified)"
domain: [heuristics]
priority: ✦
status: standard
tags: [paper, heuristics]
---

# Primal Heuristics for Mixed-Integer Nonlinear Programming

> Extends the MIP primal-heuristic toolbox (FP, diving, LNS) to MINLP — the roadmap for the later NLP/MINLP extension.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold |
| Year | 2025 |
| Venue | Chapter in *Primal Heuristics in Integer Programming* (Cambridge) |
| DOI/URL | (unverified; Cambridge link in list) |

## Problem Addressed
MINLP solvers inherit MIP heuristics wholesale and they fail: projections onto nonlinear constraints are not LPs, rounding interacts with curvature, and feasibility is harder to repair.

## Core Contribution
- **Methodology:** Adapt FP (projection onto nonlinear relaxations), rounding (with nonlinear feasibility checks) and LNS neighborhoods to MINLP; identify which pieces transfer and which need nonlinear sub-solvers.
- **Assumptions:** Solvable NLP relaxations; local optimality acceptable for heuristics.
- **Benchmarks/datasets:** MINLP test sets (not itemized in list).
- **Metrics:** Primal integral on MINLP; success rate of adapted heuristics.
- **Key results:** Maps the transfer limits of each heuristic family — useful as a *design constraint* for keeping our current heuristics LP-shaped so they remain usable after extension.

## Engineering-Relevant Knowledge
**Algorithms:** MINLP variants of FP, rounding, diving, LNS.
**Techniques:** Projection onto nonlinear sets (SQP/IPM sub-solves); nonlinear feasibility tolerance handling.
**Implementation details:** Directly out of initial scope (R3 says NLP/MINLP later only). Relevant now only as advice: keep `src/milp/heuristics.cpp` dependent on a narrow LP interface so an NLP backend can be swapped in.
**Equations/rules:** none required now.
**Limitations/failure cases:** Nonconvexity ⇒ local minima; sub-solve cost per heuristic call is high.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Extension-path note (R3); explicitly not required by the PS for the initial scope (NLP/MINLP are "later extension" only).

## Evidence → Engineering Decision
- *Finding:* MINLP heuristics need nonlinear projections → *PS requirement:* R3 → *Component:* src/milp/heuristics.cpp (keep LP interface narrow) → *Metric:* n/a (architecture), [[Relative Optimality Gap]]

## Related Papers
- [[Mexi-2026-Frank-Wolfe-based-Primal]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2006-Primal-Heuristics-Mixed]]
- [[Fischetti-2005-Feasibility-Pump]]

## Uses
- [[Feasibility Pump]] [[Diving]] [[Rounding Heuristic]] [[LP Relaxation]]
