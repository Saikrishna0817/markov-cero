---
type: research-gap
tags: [research-gaps, parallel]
status: stable
verified_on: 2026-09-25
---

# Parallel Scalability Gap

> R7 promises multi-core scaling; the measurement shows a slowdown — the distance between the claim and the evidence.

## Definition
This gap covers everything between "threads exist in the code" and "wall-clock time improves with threads": current evidence shows negative speedup at p = 4, so there is no demonstrated scaling curve at all, positive or flat. Closing it requires both implementation work (work distribution, contention reduction, deterministic measurement) and experimental work (p-sweep on instances large enough that per-node work amortizes coordination, with idle/lock diagnostics). The gap is framed by parallel B&B literature: centralized vs decentralized control, stealing, ramp-up, redundancy management.

## Why It Matters Here
- R7 (multi-core parallelization) and R20 (industrial-scale times) are both routed through this gap; audit classifies R7 as a **REGRESSION** (docs/audit/00-ground-truth.md divergence table).
- Observed state: 4-thread speedup 0.56× / efficiency 14.1% on stein9.mps (evidence/benchmarks/phase4.json); shared mutex queue, serial root phase, no stealing (src/milp/parallel_tree_search.cpp).
- Inference: the measured instance is small (stein9), so part of the gap may be granularity rather than architecture — this must be *tested* on larger instances before redesigning.

## Key Facts / Rules
- Evidence needed: S_p/E_p for p = 1..N on ≥3 instances of differing size, with node counts and idle fractions recorded.
- Design levers: decentralized queues/work stealing, subtree ownership, batched incumbent broadcast, serial-root reduction (parallel root LP/cuts).
- Pitfalls to avoid: Lai–Sahni order anomalies, nondeterministic results without a documented determinism mode, measuring on instances too small to parallelize.
- Acceptance bar: E_p ≥ some stated threshold (or an honest characterization of where scaling stops and why).

## Related
- [[Negative Parallel Scaling]]
- [[Parallel Speedup]]
- [[Parallel Efficiency]]
- [[Work Stealing]]
- [[Eckstein-1994-Control-Strategies-Parallel]]
- [[Zhang-n.d.-On-Design-Parallel-Branch]]

## Referenced By

- [[Negative Parallel Scaling|research/limitations/Negative Parallel Scaling]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Parallel Efficiency|research/metrics/Parallel Efficiency]]