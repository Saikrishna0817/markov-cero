---
type: limitation
tags: [limitations, benchmark]
status: stable
verified_on: 2026-09-25
---

# No External Baseline

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Every timing in this repo is ours alone — so "faster" and "competitive" currently have no referent.

## Definition
An external baseline is timing (and accuracy) data from an established solver — commercial or open-source — run on the same instances, limits, hardware and tolerance settings as this solver's runs. Without it, absolute runtimes are uninterpretable: 39.7 ms could be excellent or embarrassing depending on what CPLEX/Gurobi/Xpress/HiGHS/CBC/SCIP do on the same box. A baseline is also what turns per-instance numbers into the aggregates evaluators expect (geometric mean ratios, performance profiles), and it is a *binary* deliverable in R16 — either the comparison exists or the requirement fails.

## Why It Matters Here
- R16 is one of the SIH evaluation criteria stated as a hard expectation: "compared against at least one established commercial or open-source solver".
- Observed state: docs/audit/00-ground-truth.md C.3 records **no comparison artifacts anywhere** in `evidence/`, `reports/` or `benchmarks/` — PS-GAP-03 is open; the Dolan–Moré paper note observes that `benchmarks/runners` produces only our times, so no profile matrix can be built.
- Inference: this limitation gates the presentation of *every other* performance claim — a faster engine with no baseline still scores zero on R16.

## Key Facts / Rules
- Fair baseline: same instance set, time/memory limits, thread count, hardware record, tolerance alignment (or documented differences).
- Sources: open-source (HiGHS, CBC, SCIP) remove licensing barriers; commercial availability must be declared honestly.
- Aggregate correctly: geometric mean of ratios + performance profile; give timeouts a penalty time.
- Also compare *accuracy*: a solver that finishes first with a looser gap is not faster, it is different.

## Related
- [[Missing External Baseline Comparison]]
- [[Geometric Mean Runtime]]
- [[Mittelmann Benchmarks]]
- [[MIPLIB]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]

## Referenced By

- 16-testing-evaluation-strategy
- 17-sih-demo-strategy
- 21-traceability
- Architecture MOC
- Evaluation MOC
- Research MOC
- [[ED-001-comparison-harness-before-new-algorithms|research/engineering-decisions/ED-001-comparison-harness-before-new-algorithms]]
- cross-paper-synthesis
- research-dependency-map
- [[Missing External Baseline Comparison|research/research-gaps/Missing External Baseline Comparison]]