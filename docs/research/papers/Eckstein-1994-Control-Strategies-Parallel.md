---
type: paper
title: "Control Strategies for Parallel Mixed Integer Branch and Bound"
authors: "Eckstein"
year: 1994
venue: "ACM"
doi: "10.1145/602783.602785"
domain: [parallel]
priority: ★
status: deep
tags: [paper, parallel]
---

# Control Strategies for Parallel Mixed Integer Branch and Bound

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Classifies centralized vs. decentralized work distribution for parallel branch-and-bound and shows the control policy — not the processor count — decides whether parallelism pays.

## Metadata
| Field | Value |
|---|---|
| Authors | Eckstein |
| Year | 1994 |
| Venue | ACM |
| DOI/URL | 10.1145/602783.602785 |

## Problem Addressed
Parallel B&B must decide how open nodes are handed to workers, how global bound improvements propagate, and how workers behave when the pool empties. Naive policies either starve workers or re-explore subtrees a serial run would have pruned. The paper asks which control structure yields real speedup rather than organized waste.

## Core Contribution
- **Methodology:** Taxonomy of control strategies: centralized broker vs. decentralized work pools; Karp–Zhang randomization of node selection; rendezvous vs. wait-free bound exchange; incumbent (best bound) broadcast protocols; priority schemes for selecting which node to expand next.
- **Assumptions:** Workers evaluate complete LP relaxations; the global bound is monotone; coordination messages are far cheaper than a node expansion.
- **Benchmarks/datasets:** Mid-1990s MILP test sets (exact instance lists not re-verified here).
- **Metrics:** Wall-clock speedup, duplicated/wasted node work, idle fraction, sensitivity to incumbent-broadcast frequency.
- **Key results:** Randomized decentralized scheduling with periodic incumbent exchange is robust against pathological serial-equivalent orderings; centralized brokers degrade as per-node arrival rate grows (figures not re-verified).

## Engineering-Relevant Knowledge
**Algorithms:** Central node queue vs. per-worker deques with stealing; randomized tie-breaking among near-equal bound nodes; global-bound gossip.
**Techniques:** Rendezvous protocol (bounded staleness of bound information), wait-free polling intervals, pruning on arrival of an improved incumbent without a global barrier.
**Implementation details:** Decentralized pools avoid a global lock on every pop — the same pressure our `src/milp/work_queue.cpp` faces; broadcast pruning must be cheap or it dominates when trees are tiny.
**Equations/rules:** Speedup S_p = T_1/T_p; an anomaly (S_p < 1) arises when parallel workers explore subtrees serial search would prune — not merely from load imbalance.
**Limitations/failure cases:** On a tiny tree, fixed coordination cost guarantees S_p < 1 no matter how good the policy is — exactly the regime of our current measurement.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Our 4-thread B&B measured 0.56× speedup; Eckstein's taxonomy is the design space for our node-distribution policy and tells us where a slowdown is structural (tree too small) versus policy-induced (broker/stale bounds).

## Evidence → Engineering Decision
- *Finding:* 4-thread run gives speedup_4th 0.56×, efficiency 14.1% on stein9.mps (evidence/benchmarks/phase4.json) → *PS requirement:* R7 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* [[Parallel Speedup]]
- *Finding:* Per-node coordination and incumbent checks dominate when the tree has tens of nodes → *PS requirement:* R7 → *Component:* src/milp/work_queue.cpp → *Metric:* [[Parallel Efficiency]]

## Related Papers
- [[Lai-1984-Anomalies-Parallel-Branch]]
- [[Berthold-2019-Parallel-SCIP-UG]]
- [[Zhang-n.d.-On-Design-Parallel-Branch]]
- [[Censor-1997-Parallel-Optimization]]

## Uses
- [[Work Stealing]]
- [[Branch and Bound]]
- [[Parallel Speedup]]
