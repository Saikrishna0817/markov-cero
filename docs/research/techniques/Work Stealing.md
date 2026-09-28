---
type: concept
tags: [techniques, parallel]
status: stable
verified_on: 2026-09-25
---

# Work Stealing

> Idle workers pull nodes from busy ones instead of waiting — the standard fix for load imbalance in parallel branch-and-bound.

## Definition
Work stealing lets a thread that empties its local queue request nodes (or whole subtrees) from peers that still have work, typically via per-thread deques where thieves steal from the opposite end (Chase–Lev style) or via a global pool with distributed ownership. In branch-and-bound the imbalance is structural: one deep subtree can dominate wall-clock while other workers starve, so a purely static or purely central distribution wastes cores. Stealing interacts with correctness: stolen nodes must carry enough bound/feasibility context, and incumbent propagation must be prompt or thieves re-explore work already prunable.

## Why It Matters Here
- R7 (multi-core parallelization) is currently *negative* — evidence/benchmarks/phase4.json reports 4-thread speedup 0.56× (efficiency 14%) — and load imbalance plus lock contention are the first suspects.
- Observed state: the parallel driver uses a single shared `ThreadSafeNodeQueue` (mutex + condition_variable min-heap) that workers pop from (include/markov_cero/milp/work_queue.hpp:18-57) — Inference: that is a central-pool design, not stealing, so every pop contends on one mutex and idle-time distribution depends entirely on the global heap.
- Related design space (centralized broker vs decentralized stealing) is exactly Eckstein 1994's taxonomy and Zhang et al.'s parallel B&B design paper.

## Key Facts / Rules
- Steal granularity: whole subtrees amortize synchronization; individual nodes balance better but churn the pool.
- Synchronization cost must stay ≪ node LP cost — on small instances stealing cannot pay (Lai–Sahni anomalies).
- Incumbent broadcast frequency directly determines wasted work: stale bounds ⇒ duplicated exploration.
- Determinism requires a canonical tie-breaking/ordering policy even when the schedule is nondeterministic (see [[Deterministic Reduction]]).

## Related
- [[Parallel Speedup]]
- [[Parallel Efficiency]]
- [[Branch and Bound]]
- [[Negative Parallel Scaling]]
- [[ParallelTreeSearch]]
- [[Eckstein-1994-Control-Strategies-Parallel]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[ED-006-load-balanced-parallel-or-demote|research/engineering-decisions/ED-006-load-balanced-parallel-or-demote]]
- [[Negative Parallel Scaling|research/limitations/Negative Parallel Scaling]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Parallel Efficiency|research/metrics/Parallel Efficiency]]
- [[Parallel Speedup|research/metrics/Parallel Speedup]]
- [[Abbasi-2020-PIPS-PSBB-Multi-Level|research/papers/Abbasi-2020-PIPS-PSBB-Multi-Level]]
- [[Berthold-2019-Parallel-SCIP-UG|research/papers/Berthold-2019-Parallel-SCIP-UG]]
- [[Eckstein-1994-Control-Strategies-Parallel|research/papers/Eckstein-1994-Control-Strategies-Parallel]]
- [[Zhang-n.d.-On-Design-Parallel-Branch|research/papers/Zhang-n.d.-On-Design-Parallel-Branch]]
- [[Parallel Scalability Gap|research/research-gaps/Parallel Scalability Gap]]
- [[Deterministic Reduction|research/techniques/Deterministic Reduction]]