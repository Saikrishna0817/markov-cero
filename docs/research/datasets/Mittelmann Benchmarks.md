---
type: concept
tags: [datasets, benchmark]
status: stable
verified_on: 2026-09-25
---

# Mittelmann Benchmarks

> The live leaderboards SIH evaluators can check in one click — geometric means, matched hardware, updated continuously.

## Definition
Hans Mittelmann's benchmark pages (Arizona State University, plato.asu.edu) maintain continuously updated performance comparisons of optimization software on LP, MIP and QP instance sets, using aggregated metrics — typically geometric mean runtimes or shifted-geometric means — over standardized sets with consistent time limits and hardware notes. The methodology matters as much as the numbers: same instance set for every solver, explicit handling of timeouts, and ratios rather than raw averages so no single instance dominates. A solver that appears (or is deliberately absent) from these pages is judged by a methodology the community already trusts.

## Why It Matters Here
- R15 names Mittelmann benchmark sets; R16's "compare against at least one established solver" is naturally satisfied by running the same public sets the leaderboards use.
- Observed state: **no Mittelmann instance sets are present in the repo** (PS-GAP-04, docs/audit/00-ground-truth.md) — so R15 is only partially met today.
- Inference: adopting Mittelmann's aggregation style (geometric mean of ratios) would also fix the current habit of reporting single-instance times.

## Key Facts / Rules
- Aggregation: geometric mean of per-instance ratios (or shifted geometric mean of times) — not arithmetic mean.
- Requirements for a fair run: identical instance set, stated time/memory limits, recorded hardware, explicit timeout penalty.
- Live pages cover LP, MILP and QP separately — a QP claim needs the QP set, not only LP results.
- Absence from a public leaderboard is itself an evaluation signal; presence requires reproducible artifacts.

## Related
- [[Geometric Mean Runtime]]
- [[Mittelmann Coverage Gap]]
- [[Missing External Baseline Comparison]]
- [[Netlib LP Collection]]
- [[Mittelmann-n.d.-Mittelmann-LP-MILP]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Datasets MOC|research/Datasets MOC]]
- [[Research MOC|research/Research MOC]]
- [[Netlib LP Collection|research/datasets/Netlib LP Collection]]
- [[No External Baseline|research/limitations/No External Baseline]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Geometric Mean Runtime|research/metrics/Geometric Mean Runtime]]
- [[Relative Optimality Gap|research/metrics/Relative Optimality Gap]]
- [[Mittelmann-n.d.-Benchmarks-Optimization-Software|research/papers/Mittelmann-n.d.-Benchmarks-Optimization-Software]]
- [[Mittelmann-n.d.-Mittelmann-LP-MILP|research/papers/Mittelmann-n.d.-Mittelmann-LP-MILP]]
- [[Missing External Baseline Comparison|research/research-gaps/Missing External Baseline Comparison]]
- [[Mittelmann Coverage Gap|research/research-gaps/Mittelmann Coverage Gap]]