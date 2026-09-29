---
type: paper
title: "Parallel SCIP / UG Framework"
authors: "Berthold; Bussieck, Lindner, Ludwig et al."
year: 2019
venue: "Mathematical Programming Computation"
doi: "(unverified)"
domain: [parallel]
priority: ★
status: deep
tags: [paper, parallel]
---

# Parallel SCIP / UG Framework

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Reference design for malleable/distributed parallel branch-and-bound: ramp-up, node exchange between workers, and a determinism contract.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold; Bussieck, Lindner, Ludwig et al. |
| Year | 2019 |
| Venue | Mathematical Programming Computation (as listed) |
| DOI/URL | (unverified) |

## Problem Addressed
Turning a serial MIP solver into a parallel one requires more than threads: work must be redistributed as the tree shape changes, the incumbent must stay consistent, and users must be able to reproduce runs. The entry describes how SCIP's parallel mode and the UG wrapper framework solve these for general MIP solvers.

## Core Contribution
- **Methodology:** Malleable worker pool (threads/processes join and leave), ramp-up phase that grows the parallel subtree footprint early, node exchange via message passing between workers, and an optional deterministic execution mode that replays a fixed decision order.
- **Assumptions:** LP subproblem solver is re-entrant; a single global incumbent exists; non-deterministic scheduling is acceptable by default for speed.
- **Benchmarks/datasets:** MIPLIB-style instance sets used by SCIP/UG papers (specific tables not re-verified here).
- **Metrics:** Wall-clock speedup, nodes explored per second, time to first incumbent, run-to-run reproducibility.
- **Key results:** Ramp-up + dynamic exchange approach serial-efficiency levels on large instances; superlinear speedups are reported for some instances because a wider search is feasible in the same wall-clock budget ((approximate), not re-verified); deterministic mode costs extra synchronization.

## Engineering-Relevant Knowledge
**Algorithms:** Dynamic node pools with work stealing, ramp-up subtree assignment, distributed bound/incumbent propagation.
**Techniques:** Local pools + steal requests instead of one global queue; deterministic replay ordering for reproducible benchmarks; incumbent re-check on pop to avoid solving stale nodes.
**Implementation details:** Maps onto our `parallel_tree_search.cpp` (pool + worker loop), `work_queue.cpp` (steal/queue policy) and `shared_incumbent.cpp` (bound publication); a deterministic mode would let us A/B changes without run-to-run noise.
**Equations/rules:** Report both S_p = T_1/T_p and efficiency E_p = S_p/p; SCIP practice is also to report nodes-explored inflation versus serial runs to separate "more work" from "faster work".
**Limitations/failure cases:** Ramp-up assumes a large enough tree to subdivide; with a tree of ~40 nodes (stein9) the framework has nothing to ramp up on, so overhead dominates.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the closest architectural blueprint to what our parallel B&B must become (malleable pool, node exchange, determinism), and it is built from scratch around a serial solver — the same migration path R7 requires of us.

## Evidence → Engineering Decision
- *Finding:* Measured 4-thread speedup 0.56× / efficiency 14.1% on a tiny instance (evidence/benchmarks/phase4.json) → *PS requirement:* R7 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* [[Parallel Speedup]]
- *Finding:* phase4 records identical_optima across thread counts, i.e. no determinism contract is enforced beyond outcome equality → *PS requirement:* R15 → *Component:* src/milp/shared_incumbent.cpp → *Metric:* run-to-run reproducibility of objective and node count

## Related Papers
- [[Eckstein-1994-Control-Strategies-Parallel]]
- [[Zhang-n.d.-On-Design-Parallel-Branch]]
- [[Schweizer-2018-Deterministic-Parallel-MIP]]
- [[Lai-1984-Anomalies-Parallel-Branch]]

## Uses
- [[Work Stealing]]
- [[Parallel Efficiency]]
- [[Branch and Bound]]
