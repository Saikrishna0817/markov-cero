---
type: metric
tags: [metrics, parallel]
status: stable
verified_on: 2026-09-25
---

# Parallel Speedup

> S_p = T_1/T_p — the number that is supposed to go up with threads, and currently does not.

## Definition
Parallel speedup compares the wall-clock time of a serial run T_1 against the same workload on p processors T_p: S_p = T_1/T_p. Superlinear speedup (> p) can occur from cache effects or (in branch-and-bound) from a different search order finding better incumbents earlier; *sub*-linear speedup is normal, and S_p < 1 means parallelization made things worse. The measurement must use the same instance, limits and termination criteria on both runs, and for branch-and-bound the node counts should be reported alongside, since a changed search order changes the work being timed.

## Why It Matters Here
- R7 (multi-core parallelization) is directly graded by this number.
- Observed state: `evidence/benchmarks/phase4.json` records 4-thread speedup **0.56×** on stein9.mps — a slowdown, i.e. R7 effectively unmet (docs/audit/00-ground-truth.md C.3, and the R7 row of the divergence table).
- Inference: candidate causes include single-mutex queue contention, tiny per-node LP cost vs synchronization overhead (Lai–Sahni anomaly), and no work stealing (see [[Work Stealing]]).

## Key Facts / Rules
- S_p = T_1/T_p with identical work limits; report node counts to separate "less work" from "same work slower".
- S_p < 1 ⇒ structural or policy-induced slowdown — diagnose with idle fraction and lock-wait measurements.
- Superlinear is possible in B&B via better incumbents earlier (search-order effect), not via hardware magic.
- Amdahl: serial root phase (LP + cuts + strong branching) bounds the maximum achievable S_p here, since it runs before workers start (src/milp/parallel_tree_search.cpp:315-447).

## Related
- [[Parallel Efficiency]]
- [[Work Stealing]]
- [[Negative Parallel Scaling]]
- [[Geometric Mean Runtime]]
- [[Eckstein-1994-Control-Strategies-Parallel]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[16-testing-evaluation-strategy|audit/16-testing-evaluation-strategy]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Evaluation MOC|research/Evaluation MOC]]
- [[Research MOC|research/Research MOC]]
- [[Negative Parallel Scaling|research/limitations/Negative Parallel Scaling]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Geometric Mean Runtime|research/metrics/Geometric Mean Runtime]]
- [[Parallel Efficiency|research/metrics/Parallel Efficiency]]
- [[Achterberg-2007-Best-Estimate-Bound|research/papers/Achterberg-2007-Best-Estimate-Bound]]
- [[Berthold-2019-Parallel-SCIP-UG|research/papers/Berthold-2019-Parallel-SCIP-UG]]
- [[Eckstein-1994-Control-Strategies-Parallel|research/papers/Eckstein-1994-Control-Strategies-Parallel]]
- [[Fischetti-2015-Improving-Branch-Cut|research/papers/Fischetti-2015-Improving-Branch-Cut]]
- [[Helbecque-2025-PGAS-Based-Parallel-Branch|research/papers/Helbecque-2025-PGAS-Based-Parallel-Branch]]
- [[Lai-1984-Anomalies-Parallel-Branch|research/papers/Lai-1984-Anomalies-Parallel-Branch]]
- [[Lodi-2013-Performance-Variability-Mixed|research/papers/Lodi-2013-Performance-Variability-Mixed]]
- [[Schweizer-0000-Restart-Strategies-MIP|research/papers/Schweizer-0000-Restart-Strategies-MIP]]
- [[Zhang-n.d.-On-Design-Parallel-Branch|research/papers/Zhang-n.d.-On-Design-Parallel-Branch]]
- [[GPU Benefit Unproven|research/research-gaps/GPU Benefit Unproven]]
- [[Parallel Scalability Gap|research/research-gaps/Parallel Scalability Gap]]
- [[Deterministic Reduction|research/techniques/Deterministic Reduction]]
- [[Work Stealing|research/techniques/Work Stealing]]