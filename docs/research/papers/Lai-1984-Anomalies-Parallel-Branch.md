---
type: paper
title: "Anomalies in Parallel Branch-and-Bound Algorithms"
authors: "Lai & Sahni"
year: 1984
venue: "ACM"
doi: "(unverified)"
domain: [parallel]
priority: ✦
status: standard
tags: [paper, parallel]
---
# Anomalies in Parallel Branch-and-Bound Algorithms

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Proves that adding processors can *increase* total work and wall-clock time in branch-and-bound — the theoretical reason a parallel solver can be slower than serial.
## Metadata
| Field | Value |
|---|---|
| Authors | Lai & Sahni |
| Year | 1984 |
| Venue | ACM |
| DOI/URL | https://dl.acm.org/doi/10.1145/358080.358071 |

## Problem Addressed
Parallel B&B workers expand nodes that a serial run would already have pruned by an improved incumbent discovered elsewhere. The paper formalizes when parallel schedules cause "anomalies": superlinear speedup (a win for luck-based ordering) or, more dangerously, slowdown where more processors mean more total work.
## Core Contribution
- **Methodology:** Schedule-theoretic analysis of parallel B&B; constructs executions where p processors explore strictly more nodes than one; relates anomalies to when bounds improve relative to exploration order.
- **Assumptions:** Nodes are expanded atomically; bound information reaches workers with delay (no instantaneous global bound).
- **Benchmarks/datasets:** Constructed examples plus scheduling analysis (no instance benchmark suite).
- **Metrics:** Total nodes explored, wall-clock time, speedup S_p (anomaly if S_p < 1 or if work grows with p).
- **Key results:** Both anomaly directions are possible; anomaly avoidance requires controlling exploration order, not just adding workers.
## Engineering-Relevant Knowledge
**Algorithms:** Parallel B&B anomaly analysis; ordering-dependent pruning.
**Techniques:** Bound broadcast frequency and randomized node selection reduce the chance of pathological orders.
**Implementation details:** Explains why our shared-queue B&B on stein9 reports speedup_4th 0.56×: with ~40 open nodes, four workers over-expand while the incumbent propagates late (`src/milp/shared_incumbent.cpp`).
**Limitations/failure cases:** Any design that spawns workers without a minimum-work-to-worker ratio invites this class of slowdown.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It reframes our 0.56× not as a bug but as a predictable outcome of tree size versus coordination, so the fix is policy/gating (don't parallelize tiny trees), not just faster queues.
## Evidence → Engineering Decision
- *Finding:* 4-thread speedup 0.56×, efficiency 14.1% on stein9.mps (evidence/benchmarks/phase4.json) → *PS requirement:* R7 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* [[Parallel Speedup]]
## Related Papers
- [[Eckstein-1994-Control-Strategies-Parallel]]
- [[Zhang-n.d.-On-Design-Parallel-Branch]]
## Uses
- [[Branch and Bound]]
