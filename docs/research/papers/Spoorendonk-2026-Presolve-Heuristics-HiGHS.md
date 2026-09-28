---
type: paper
title: "Presolve Heuristics in HiGHS: Implementation and Computational Study"
authors: "Spoorendonk"
year: 2026
venue: "(not stated in list)"
doi: "(unverified)"
domain: [heuristics]
priority: ✦
status: standard
tags: [paper, heuristics]
---

# Presolve Heuristics in HiGHS: Implementation and Computational Study

> Reference implementations of modern cheap heuristics (Feasibility Jump, fix-propagate-repair, LocalMIP, Scylla) with a reported +23.7% primal-integral gain.

## Metadata
| Field | Value |
|---|---|
| Authors | Spoorendonk |
| Year | 2026 |
| Venue | (not stated in list; arXiv preprint) |
| DOI/URL | arXiv:2609.22938 (link in list) |

## Problem Addressed
Small heuristics run during presolve are often skipped as insignificant, yet first-incumbent quality drives the whole tree. The paper measures what these cheap heuristics are worth in a modern open solver.

## Core Contribution
- **Methodology:** Implement Feasibility Jump, fix-propagate-repair, LocalMIP and Scylla inside HiGHS presolve; evaluate contribution to the primal integral against the baseline.
- **Assumptions:** HiGHS presolve infrastructure; standard benchmark sets.
- **Benchmarks/datasets:** Standard MIP benchmark sets (as used by HiGHS).
- **Metrics:** Primal integral (primary), time to first incumbent, solve time.
- **Key results:** ~+23.7% primal-integral improvement (per list) from cheap heuristics alone — concrete evidence that incumbent quality, not just cuts/branching, moves the needle.

## Engineering-Relevant Knowledge
**Algorithms:** Feasibility Jump (see #118), fix-propagate-repair, LocalMIP, Scylla.
**Techniques:** Running heuristics *inside* presolve, before search starts; incremental repair without LP.
**Implementation details:** Actionable spec for `src/milp/heuristics.cpp`: run FP + a jump-style repair immediately after `src/presolve/presolve.cpp`, before the first LP. Note this is HiGHS-based code — our solver must reimplement (R10 from-scratch constraint).
**Equations/rules:** Primal integral = ∫ gap(t) dt (the metric we should start reporting for R16).
**Limitations/failure cases:** Results are HiGHS-specific; some heuristics depend on its propagator, which we lack.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Supplies both the missing heuristic recipes and the metric (primal integral) we should adopt for reporting incumbent quality against a comparison solver.

## Evidence → Engineering Decision
- *Finding:* cheap pre-search heuristics yield +23.7% primal integral → *PS requirement:* R5, R16, R20 → *Component:* src/milp/heuristics.cpp → *Metric:* primal integral, time-to-first-incumbent

## Related Papers
- [[Berthold-2023-Feasibility-Jump]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Achterberg-2007-Improving-Feasibility-Pump]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics]]

## Uses
- [[Feasibility Pump]] [[Rounding Heuristic]] [[Diving]] [[Relative Optimality Gap]]
