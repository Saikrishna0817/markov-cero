---
type: metric
tags: [metrics, parallel]
status: stable
verified_on: 2026-09-25
---

# Parallel Efficiency

> E_p = S_p/p — speedup divided by threads, so 4 threads on a 0.56× speedup reads as 14%, not as "parallel".

## Definition
Parallel efficiency normalizes speedup by the processor count: E_p = S_p/p = T_1/(p·T_p), interpreted as the fraction of ideal linear scaling achieved. E = 100% means perfect linear speedup; E ≥ 100% signals superlinear effects (cache or search-order); low E means overhead, load imbalance or idle time dominate. Reporting E alongside S_p makes scaling degradation visible at a glance — a 0.56× speedup at p = 4 is only legible as E = 14% when both are shown. Weak and strong scaling variants use different normalizations: strong scaling holds the problem fixed (this definition), weak scaling holds work per processor fixed.

## Why It Matters Here
- R7's "multi-core parallelization" claim needs E_p over a range of thread counts, not a single run.
- Observed state: phase4.json shows efficiency **14.1%** at p = 4 (docs/audit/00-ground-truth.md C.3; cited in the Eckstein and Berthold paper notes) — the metric that converts the abstract "R7 unmet" into a number.
- Inference: measuring E at p = 2, 4, 8 on a fixed instance set is the minimum evidence needed to show whether the curve is policy-limited (fixable) or workload-limited (needs bigger instances).

## Key Facts / Rules
- E_p = S_p/p; report the p-sweep, not one point, and record instance + node counts.
- Serial fraction (root LP, cuts, strong branching, joins) enters via Amdahl's law and caps E_p.
- For B&B, efficiency confounds search-order effects with pure overhead — report nodes/sec too.
- Deterministic mode (see [[Deterministic Reduction]]) allows run-to-run comparison without scheduling noise.

## Related
- [[Parallel Speedup]]
- [[Negative Parallel Scaling]]
- [[Work Stealing]]
- [[Parallel Scalability Gap]]
- [[Lai-1984-Anomalies-Parallel-Branch]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[16-testing-evaluation-strategy|audit/16-testing-evaluation-strategy]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Evaluation MOC|research/Evaluation MOC]]
- [[Negative Parallel Scaling|research/limitations/Negative Parallel Scaling]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Parallel Speedup|research/metrics/Parallel Speedup]]
- [[Abbasi-2020-PIPS-PSBB-Multi-Level|research/papers/Abbasi-2020-PIPS-PSBB-Multi-Level]]
- [[Anderson-1989-Solving-Sparse-Linear|research/papers/Anderson-1989-Solving-Sparse-Linear]]
- [[Berthold-2019-Parallel-SCIP-UG|research/papers/Berthold-2019-Parallel-SCIP-UG]]
- [[Censor-1997-Parallel-Optimization|research/papers/Censor-1997-Parallel-Optimization]]
- [[Eckstein-1994-Control-Strategies-Parallel|research/papers/Eckstein-1994-Control-Strategies-Parallel]]
- [[Zhang-n.d.-On-Design-Parallel-Branch|research/papers/Zhang-n.d.-On-Design-Parallel-Branch]]
- [[Parallel Scalability Gap|research/research-gaps/Parallel Scalability Gap]]
- [[Work Stealing|research/techniques/Work Stealing]]