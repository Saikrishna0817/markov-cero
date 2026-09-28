---
type: metric
tags: [metrics, benchmark]
status: stable
verified_on: 2026-09-25
---

# Geometric Mean Runtime

> exp(mean(ln t)) — the aggregate that keeps one 1000-second instance from erasing every other result.

## Definition
The geometric mean of n positive runtimes t_i is GM = exp((1/n)·Σ ln t_i) = (Π t_i)^(1/n); equivalently, for solver *ratios* r_i = t_i/t_i*, the geometric mean of the ratios answers "on average, how many times slower than the reference". Because it is computed in log space, it is dominated by ratios rather than by absolute outliers, which is why it is the default aggregation for cross-instance solver comparisons (Mittelmann pages, solver papers) while arithmetic means are not. Timeouts require an explicit penalty time before the log is taken, otherwise censored runs silently bias the aggregate.

## Why It Matters Here
- R16 requires a comparison against an established solver, and R15/R20 require results across benchmark suites — both are graded on aggregates, not single instances.
- Observed state: evidence files hold per-instance times (evidence/netlib_results.csv: 0.53–39.7 ms rows; miplib_results.csv) but Inference: no geometric-mean-vs-baseline aggregation is computed anywhere, since no baseline column exists (see [[Missing External Baseline Comparison]]).
- Inference: reporting per-instance times without an aggregate invites exactly the outlier domination Dolan & Moré warn about.

## Key Facts / Rules
- GM = exp(mean(ln t)); ratios: GM(t_A/t_B) over the common instance set.
- Assign a fixed penalty time to timeouts/failures before taking logs; document the penalty.
- Geometric mean of ratios ≈ 1 means parity; it is scale-free and instance-set dependent — never compare GMs across different instance sets.
- Companion presentation: performance profile (Dolan & Moré 2002) shows the distribution behind the scalar.

## Related
- [[Mittelmann Benchmarks]]
- [[Missing External Baseline Comparison]]
- [[Parallel Speedup]]
- [[MIPLIB]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[16-testing-evaluation-strategy|audit/16-testing-evaluation-strategy]]
- [[21-traceability|audit/21-traceability]]
- [[missing-hardware-metadata-in-evidence|codebase/technical-debt/missing-hardware-metadata-in-evidence]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Datasets MOC|research/Datasets MOC]]
- [[Evaluation MOC|research/Evaluation MOC]]
- [[Research MOC|research/Research MOC]]
- [[MIPLIB|research/datasets/MIPLIB]]
- [[Mittelmann Benchmarks|research/datasets/Mittelmann Benchmarks]]
- [[Netlib LP Collection|research/datasets/Netlib LP Collection]]
- [[ED-001-comparison-harness-before-new-algorithms|research/engineering-decisions/ED-001-comparison-harness-before-new-algorithms]]
- [[No External Baseline|research/limitations/No External Baseline]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Parallel Speedup|research/metrics/Parallel Speedup]]
- [[Achterberg-2005-Branching-Rules-Revisited|research/papers/Achterberg-2005-Branching-Rules-Revisited]]
- [[Achterberg-2007-Best-Estimate-Bound|research/papers/Achterberg-2007-Best-Estimate-Bound]]
- [[Chung-2015-Computational-Study-Cutting|research/papers/Chung-2015-Computational-Study-Cutting]]
- [[Dolan-2002-Benchmarking-Optimization-Software|research/papers/Dolan-2002-Benchmarking-Optimization-Software]]
- [[Giallombardo-2025-Machine-Learning-Techniques|research/papers/Giallombardo-2025-Machine-Learning-Techniques]]
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation|research/papers/Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Hollenbeck-2014-Important-Branching-Decisions|research/papers/Hollenbeck-2014-Important-Branching-Decisions]]
- [[Lin-2025-PDCS-Primal-Dual|research/papers/Lin-2025-PDCS-Primal-Dual]]
- [[Linderoth-2000-Impact-Branch-Bound|research/papers/Linderoth-2000-Impact-Branch-Bound]]
- [[Lodi-2013-Performance-Variability-Mixed|research/papers/Lodi-2013-Performance-Variability-Mixed]]
- [[Mittelmann-n.d.-Benchmarks-Optimization-Software|research/papers/Mittelmann-n.d.-Benchmarks-Optimization-Software]]
- [[Mittelmann-n.d.-Mittelmann-LP-MILP|research/papers/Mittelmann-n.d.-Mittelmann-LP-MILP]]
- [[Rice-n.d.-Algorithm-Testing-Adequacy|research/papers/Rice-n.d.-Algorithm-Testing-Adequacy]]
- [[Schweizer-0000-Restart-Strategies-MIP|research/papers/Schweizer-0000-Restart-Strategies-MIP]]
- [[Turner-2024-Potential-Cutting-Planes|research/papers/Turner-2024-Potential-Cutting-Planes]]
- [[Zhang-2025-Learning-Select-Nodes|research/papers/Zhang-2025-Learning-Select-Nodes]]
- [[Missing External Baseline Comparison|research/research-gaps/Missing External Baseline Comparison]]
- [[Mittelmann Coverage Gap|research/research-gaps/Mittelmann Coverage Gap]]