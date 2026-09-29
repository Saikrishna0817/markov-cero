---
type: research-gap
tags: [research-gaps, benchmark]
status: stable
verified_on: 2026-09-25
---

# Missing External Baseline Comparison

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> R16 asks for one established solver to compare against; there is none — the highest-priority evaluator-facing gap.

## Definition
The gap between "we have results" and "we have *comparative* results": no artifact in the repository contains timings or accuracy from any solver other than this one. Closing it means selecting at least one established solver (open-source options remove the licensing question), running it on the same instances under matched settings, recording hardware, and aggregating with a defensible method. Until then, all performance statements are self-referential and the R16 criterion is unmet by construction.

## Why It Matters Here
- R16 is listed as one of the *binary* evaluator-facing gaps in docs/audit/00-ground-truth.md (section D.2 and PS-GAP-03) — it cannot be argued away, only closed or explicitly re-scoped with evidence.
- Observed state: evidence/, reports/ and benchmarks/ contain only our own outputs (netlib_results.csv, miplib_results.csv, crossover_study.csv, gpu_benchmark.csv) — no competitor column anywhere.
- Inference: because the deliverable is binary, this gap outranks incremental performance improvements in audit priority.

## Key Facts / Rules
- Minimum viable closure: 1 baseline solver × agreed instance set × recorded hardware × matched limits × geometric-mean ratios.
- Present with performance profiles (Dolan–Moré) so outliers and failures are visible, not averaged away.
- Report accuracy alongside time (gap/KKT residual) — match the *task*, not just the clock.
- Sovereignty (R10) forbids building *on* an open-source solver, not comparing *against* one — the audit states this explicitly.

## Related
- [[No External Baseline]]
- [[Geometric Mean Runtime]]
- [[Mittelmann Benchmarks]]
- [[MIPLIB]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]

## Referenced By

- 21-traceability
- Algorithms MOC
- Evaluation MOC
- Research MOC
- [[MIPLIB|research/datasets/MIPLIB]]
- [[Mittelmann Benchmarks|research/datasets/Mittelmann Benchmarks]]
- [[ED-001-comparison-harness-before-new-algorithms|research/engineering-decisions/ED-001-comparison-harness-before-new-algorithms]]
- [[No External Baseline|research/limitations/No External Baseline]]
- cross-paper-synthesis
- research-dependency-map
- [[Geometric Mean Runtime|research/metrics/Geometric Mean Runtime]]