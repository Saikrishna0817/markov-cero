---
type: paper
title: "On the Design of Parallel Branch-and-Bound Algorithms for MIP"
authors: "Zhang, Lin, Ralphs & Lübecke; Shinano et al. (ParaSCIP)"
year: n.d.
venue: "(unverified)"
doi: "(unverified)"
domain: [parallel]
priority: ★
status: deep
tags: [paper, parallel]
---

# On the Design of Parallel Branch-and-Bound Algorithms for MIP

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Design patterns that make parallel MIP trees scale: work stealing, racing ramp-up, and redundancy management.

## Metadata
| Field | Value |
|---|---|
| Authors | Zhang, Lin, Ralphs & Lübecke; Shinano et al. (ParaSCIP) |
| Year | n.d. (no year stated in the source list — verify before citing) |
| Venue | (unverified) |
| DOI/URL | (unverified) |

## Problem Addressed
Even with a correct parallel B&B, speedup can collapse because workers duplicate subtrees, wait at barriers, or lose pruning information. The paper addresses the *design* question: which work-distribution and synchronization mechanisms let a parallel MIP solver retain serial efficiency as core counts grow.

## Core Contribution
- **Methodology:** Comparison of centralized vs. decentralized control for parallel B&B; work stealing from local pools; racing ramp-up (start several configurations/subtree splits concurrently and keep the winner); redundancy management to bound duplicated subtree exploration.
- **Assumptions:** Re-entrant LP solver; tolerable staleness of the global bound; enough tree size to give every worker a distinct subtree.
- **Benchmarks/datasets:** MIPLIB instances in the ParaSCIP/ZIB line of work (exact tables not re-verified here).
- **Metrics:** Speedup, efficiency, nodes explored vs. serial, duplicated-node ratio, ramp-up duration.
- **Key results:** Racing ramp-up plus stealing delivers near-linear and sometimes superlinear speedups on large MIPs ((approximate), not re-verified); redundancy control is what prevents slowdown on hard trees.

## Engineering-Relevant Knowledge
**Algorithms:** Work stealing, racing ramp-up, redundant-work suppression, distributed bound synchronization.
**Techniques:** Steal requests over a local deque rather than a global queue; speculative subtree duplication with cancellation once a bound invalidates it; ramp-up sized from measured node-processing rate.
**Implementation details:** Our current implementation is a single shared queue with no stealing and no ramp-up (`src/milp/work_queue.cpp`, `src/milp/parallel_tree_search.cpp`); adding steal-on-idle plus a bounded-staleness incumbent check is the smallest change that matches this literature.
**Equations/rules:** Efficiency E_p = S_p/p; redundant-work ratio = duplicated nodes / total nodes — a metric we do not yet record.
**Limitations/failure cases:** Racing ramp-up multiplies early work; on small instances (stein9, flugpl) it is pure overhead — the paper's own framing implies a minimum tree size before parallel modes are worth enabling.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It names the two mechanisms our parallel mode lacks (stealing, ramp-up) and gives the metric (redundancy) that explains why our 4-thread run explores more work than it saves.

## Evidence → Engineering Decision
- *Finding:* 4-thread B&B is 0.56× with 14.1% efficiency on stein9.mps (evidence/benchmarks/phase4.json) — a tree far below any ramp-up threshold → *PS requirement:* R7 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* [[Parallel Efficiency]]
- *Finding:* No redundant-work or steal-rate telemetry exists in the benchmark output → *PS requirement:* R15 → *Component:* benchmarks/runners → *Metric:* duplicated-node ratio alongside [[Parallel Speedup]]

## Related Papers
- [[Eckstein-1994-Control-Strategies-Parallel]]
- [[Berthold-2019-Parallel-SCIP-UG]]
- [[Lai-1984-Anomalies-Parallel-Branch]]
- [[Abbasi-2020-PIPS-PSBB-Multi-Level]]

## Uses
- [[Work Stealing]]
- [[Parallel Speedup]]
- [[Branch and Bound]]
