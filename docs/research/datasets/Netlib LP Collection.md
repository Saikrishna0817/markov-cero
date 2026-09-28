---
type: concept
tags: [datasets, benchmark]
status: stable
verified_on: 2026-09-25
---

# Netlib LP Collection

> The classic continuous test set — small, diverse, and where every LP solver's numerics get checked first.

## Definition
The Netlib LP test set is the historical collection of linear programming problems (AFIRO, E226, PILOT, DFL001 and ~100 more) contributed from real applications and numerical experiments, distributed as MPS files with known optimal values. Instances range from tiny to large and include deliberately ill-conditioned and degenerate cases, which makes the set a de facto numerical-stress test rather than only a speed test. Because most instances are small, they measure per-iteration overhead and accuracy more than they measure scalability. Reporting relative objective error against the recorded optimum is the standard correctness metric here.

## Why It Matters Here
- R15 names Netlib explicitly; R17's robustness claims need instances with known difficult numerics — Netlib provides them at no licensing cost.
- Observed state: `evidence/netlib_results.csv` — 7 instances optimal with relative error ≤ 7.9e-15 at 0.53–39.7 ms; `netlib_extended.csv` adds 5 more at 1.3–39.7 s (docs/audit/00-ground-truth.md C.3) — the strongest current correctness evidence in the repo.
- Observed state: Netlib instances are also the input to the CPU/GPU crossover table (reports/crossover_study.csv) — Inference: the set is doing double duty as both a correctness and a GPU-scaling test, but the scaling instances are small.

## Key Facts / Rules
- Correctness metric: |cᵀx − c*|/|c*| (relative objective error) plus independent feasibility checks.
- Small instances ⇒ first-order/GPU methods usually lose; do not extrapolate GPU conclusions from them.
- Ill-conditioned members are where condition estimation, scaling and refinement show their value.
- Free/public: no access barrier for reproducibility claims (unlike commercial benchmark suites).

## Related
- [[Ill-Conditioning]]
- [[KKT Residual]]
- [[Geometric Mean Runtime]]
- [[Mittelmann Benchmarks]]
- [[Anderssen-1984-NETLIB-LP-Test-Set]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[benchmark-suites|codebase/tests/benchmark-suites]]
- [[Datasets MOC|research/Datasets MOC]]
- [[Research MOC|research/Research MOC]]
- [[Mittelmann Benchmarks|research/datasets/Mittelmann Benchmarks]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Anderssen-1984-NETLIB-LP-Test-Set|research/papers/Anderssen-1984-NETLIB-LP-Test-Set]]
- [[Gamst-1970s-MPS-format-specification|research/papers/Gamst-1970s-MPS-format-specification]]
- [[Khachiyan-1979-Polynomial-Algorithm-Linear|research/papers/Khachiyan-1979-Polynomial-Algorithm-Linear]]
- [[Mittelmann Coverage Gap|research/research-gaps/Mittelmann Coverage Gap]]