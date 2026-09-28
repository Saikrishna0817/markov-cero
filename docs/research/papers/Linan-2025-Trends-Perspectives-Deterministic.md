---
type: paper
title: "Trends and Perspectives in Deterministic MINLP Optimization for Integrated Planning, Scheduling, Control and Design of Chemical Processes"
authors: "Liñán & Ricardez-Sandoval"
year: 2025
venue: "Reviews in Chemical Engineering 41(5):451–472"
doi: "10.1515/revce-2024-0064"
domain: [minlp, domain]
priority: ✦
status: standard
tags: [paper, minlp]
---
# Trends and Perspectives in Deterministic MINLP Optimization for Integrated Planning, Scheduling, Control and Design of Chemical Processes
> Deterministic MINLP methods for integrated chemical-process decisions — the review that lines up most directly with MRPL's refinery use case.
## Metadata
| Field | Value |
|---|---|
| Authors | Liñán & Ricardez-Sandoval |
| Year | 2025 |
| Venue | Reviews in Chemical Engineering 41(5):451–472 |
| DOI/URL | 10.1515/revce-2024-0064 |

## Problem Addressed
Chemical plants couple long-horizon planning/scheduling decisions with short-horizon control and design choices, giving deterministic MINLPs of industrial size. The review addresses which deterministic methods (OA, ECP, hybrid and decomposition schemes) actually scale to these integrated problems.
## Core Contribution
- **Methodology:** Survey of deterministic MINLP algorithms applied to integrated planning–scheduling–control–design: outer approximation, extended cutting plane, hybrid decomposition, and formulation strategies for process models; case studies from chemical engineering.
- **Assumptions:** Deterministic data; convex or linearized process models for the tractable classes.
- **Benchmarks/datasets:** Chemical-process case studies from literature (not re-verified).
- **Metrics:** Solution quality, computation time, scalability with horizon/discretization size.
- **Key results:** Integrated models remain computationally demanding; method choice depends on how nonlinear terms enter (objective vs. constraints) (qualitative restatement).
## Engineering-Relevant Knowledge
**Algorithms:** OA/ECP-style MINLP, decomposition for integrated process optimization.
**Techniques:** Linearizing process nonlinearities to reuse MILP machinery; tight big-M handling — a [[Scaling]] and numerical-robustness concern for our solver.
**Implementation details:** The refinery/blend models we are asked to support (R11) land exactly in this literature; even the LP/MILP subset of these case studies would be a credible industrial benchmark for R19.
**Limitations/failure cases:** Full MINLP requires NLP solvers we do not have; big-M formulations create the ill-conditioning R13 targets.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** The source list flags this as a direct match to refinery/process use cases; the LP/MILP projections of its case studies are in-scope benchmarks now, the MINLP parts are R3 roadmap.
## Evidence → Engineering Decision
- *Finding:* Refinery-class instances in-repo are 2×2 toys (phase4.json) → *PS requirement:* R19 → *Component:* src/io/mps.cpp → *Metric:* industrial-scale case study instances parsed and solved
## Related Papers
- [[Kronqvist-2025-50-Years-Mixed-Integer]]
- [[Neiro-2004-Mathematical-Modeling-Petroleum]]
## Uses
- [[Branch and Cut]]
