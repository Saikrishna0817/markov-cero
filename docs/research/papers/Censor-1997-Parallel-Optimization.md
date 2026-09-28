---
type: paper
title: "Parallel Optimization (reporting metrics)"
authors: "Censor & Zenios"
year: 1997
venue: "Oxford University Press (book)"
doi: "(unverified)"
domain: [benchmark, parallel]
priority: ○
status: standard
tags: [paper, benchmark]
---
# Parallel Optimization (reporting metrics)
> Standard definitions of strong/weak scaling — speedup and parallel efficiency — used to judge any parallel solver claim, including ours.
## Metadata
| Field | Value |
|---|---|
| Authors | Censor & Zenios |
| Year | 1997 |
| Venue | Oxford University Press (book) |
| DOI/URL | (unverified) |

## Problem Addressed
Parallel optimization papers report incomparable numbers (per-iteration speedup, end-to-end speedup, per-rank throughput). The entry supplies the standard metric definitions — strong scaling, weak scaling, speedup and efficiency — so parallel claims can be normalized and audited.
## Core Contribution
- **Methodology:** Framework for parallel algorithms in optimization (LP, network flow, projection methods) with a consistent reporting scheme: strong scaling (fixed problem, more processors) and weak scaling (problem grows with processors).
- **Assumptions:** Comparable workload per processor; wall-clock time is the currency; serial baseline clearly defined.
- **Benchmarks/datasets:** Optimization problems used in parallel case studies (not re-verified).
- **Metrics:** Speedup S_p = T_1/T_p; parallel efficiency E_p = S_p/p; weak-scaling efficiency at constant workload per rank.
- **Key results:** Efficiency loss sources enumerated: communication, load imbalance, synchronization, redundant work — the checklist for diagnosing our own numbers.
## Engineering-Relevant Knowledge
**Algorithms:** Parallel formulations of LP and network optimization.
**Techniques:** Report S_p and E_p together with problem size and processor count; state whether timing includes setup/transfer.
**Implementation details:** Directly labels our evidence: phase4.json gives speedup_4th 0.56× → E_4 = 0.141; docs/gpu.md reports transfer-inclusive GPU times (h2d/kernel/d2h), which matches this discipline.
**Limitations/failure cases:** Efficiency metrics on a single tiny instance (stein9) say little about scaling on large instances — the metric must be paired with instance scale.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R7's parallelization claim must be stated in these units; the definitions turn our anecdotal0.56× into an auditable efficiency figure and dictate how future scaling tables are written.
## Evidence → Engineering Decision
- *Finding:* 4-thread speedup 0.56× and efficiency 14.1% on stein9.mps (evidence/benchmarks/phase4.json) → *PS requirement:* R7 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* [[Parallel Efficiency]]
## Related Papers
- [[Eckstein-1994-Control-Strategies-Parallel]]
- [[Mittelmann-n.d.-Benchmarks-Optimization-Software]]
## Uses
- [[Parallel Efficiency]]
