---
type: limitation
tags: [limitations, parallel]
status: stable
verified_on: 2026-09-25
---

# Negative Parallel Scaling

> Four threads made it slower than one — 0.56× speedup, 14% efficiency — parallelization that subtracts.

## Definition
Negative parallel scaling means adding workers increases wall-clock time: S_p < 1 and E_p < 1/p, caused by synchronization overhead exceeding the useful work per unit of coordination, load imbalance leaving threads idle while one runs, duplicated work from stale bounds, or search-order anomalies where parallelism picks a worse node order (Lai & Sahni 1984 showed parallel B&B can be strictly worse than serial). In this codebase the shape is a central mutex-protected node queue plus a serial root phase, so every pop contends and the root cost is paid regardless of thread count.

## Why It Matters Here
- R7 (multi-core parallelization) is not merely unmet but *regressed*: docs/audit/00-ground-truth.md records 4-thread speedup 0.56× / efficiency 14.1% on stein9.mps (evidence/benchmarks/phase4.json) and lists R7 as a REGRESSION row.
- Observed state: workers pop from a single `ThreadSafeNodeQueue` (mutex + condition_variable) and share incumbents/pseudo-costs under locks (include/markov_cero/milp/work_queue.hpp:18-57; src/milp/parallel_tree_search.cpp:70-73) — Inference: contention + small-instance granularity explain the sign, but per-worker idle/lock-wait time has not been measured.
- Observed state: root work (LP, cuts, heuristics, strong branching) runs serially before workers start (src/milp/parallel_tree_search.cpp:315-447), capping any speedup via Amdahl.

## Key Facts / Rules
- Diagnose with: idle fraction, lock-wait time, nodes/sec, and node counts vs serial (separate overhead from search-order effects).
- Fixes, in order of leverage: work stealing / decentralized queues, coarser-grained work units, bigger instances (Amdahl + granularity), incumbent broadcast efficiency.
- Lai–Sahni anomaly: parallel search order can be inherently worse — architecture must tolerate it, not assume it away.
- Measurement hygiene: identical limits, deterministic mode where possible, report E_p across a p-sweep.

## Related
- [[Parallel Speedup]]
- [[Parallel Efficiency]]
- [[Work Stealing]]
- [[Parallel Scalability Gap]]
- [[Lai-1984-Anomalies-Parallel-Branch]]
- [[Eckstein-1994-Control-Strategies-Parallel]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[ED-006-load-balanced-parallel-or-demote|research/engineering-decisions/ED-006-load-balanced-parallel-or-demote]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Parallel Efficiency|research/metrics/Parallel Efficiency]]
- [[Parallel Speedup|research/metrics/Parallel Speedup]]
- [[Parallel Scalability Gap|research/research-gaps/Parallel Scalability Gap]]
- [[Work Stealing|research/techniques/Work Stealing]]