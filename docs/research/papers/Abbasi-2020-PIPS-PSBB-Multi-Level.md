---
type: paper
title: "PIPS-PSBB: Multi-Level Parallelism for Stochastic MIP"
authors: "Abbasi, Ralphs et al."
year: 2020
venue: "(unverified)"
doi: "(unverified)"
domain: [parallel]
priority: ○
status: standard
tags: [paper, parallel]
---
# PIPS-PSBB: Multi-Level Parallelism for Stochastic MIP
> Decentralized, MPI-collective-based node rebalancing that combines scenario-level and tree-level parallelism for stochastic mixed-integer programs.
## Metadata
| Field | Value |
|---|---|
| Authors | Abbasi, Ralphs et al. |
| Year | 2020 |
| Venue | (unverified) |
| DOI/URL | https://www.osti.gov/servlets/purl/1635781 |

## Problem Addressed
Stochastic MIPs (block-angular, many scenario copies) give two kinds of parallelism at once — across scenarios and across the branch-and-bound tree — but naive splits starve workers as the tree evolves. The paper addresses fine-grained decentralized rebalancing so both levels stay busy.
## Core Contribution
- **Methodology:** Multi-level parallel decomposition (scenario blocks + tree nodes) with decentralized node exchange implemented over MPI collectives (allgather-style pool and bound synchronization).
- **Assumptions:** Distributed memory with cheap collectives; scenario blocks independent once first-stage variables are fixed.
- **Benchmarks/datasets:** Stochastic-programming MIP test problems from the PIPS suite (exact sets not re-verified).
- **Metrics:** Speedup, efficiency, load imbalance over time, nodes/second per rank.
- **Key results:** Collective-based rebalancing keeps ranks busy near-linearly on large stochastic instances (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Parallel B&B with decentralized pool synchronization; scenario decomposition as coarse-grain parallelism.
**Techniques:** MPI collectives for bound/pool exchange; periodic rebalancing instead of per-node global locks.
**Implementation details:** Relevant only if we ever go distributed; the same "publish pool state at intervals, not per node" idea applies to our threads in `src/milp/work_queue.cpp`.
**Limitations/failure cases:** Collectives add latency that pays off only for large messages/tree sizes; useless for our tiny measured trees.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Single-node multi-core is our scope (R7), but the rebalancing pattern informs a shared-memory version of pool exchange if our current shared queue keeps scaling poorly.
## Evidence → Engineering Decision
- *Finding:* Our parallel mode uses one shared queue with no rebalancing telemetry and measures 0.56× speedup (evidence/benchmarks/phase4.json) → *PS requirement:* R7 → *Component:* src/milp/work_queue.cpp → *Metric:* [[Parallel Efficiency]]
## Related Papers
- [[Zhang-n.d.-On-Design-Parallel-Branch]]
- [[Berthold-2019-Parallel-SCIP-UG]]
## Uses
- [[Work Stealing]]
