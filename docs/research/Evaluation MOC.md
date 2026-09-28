---
type: moc
tags: [moc, evaluation, metrics, benchmarks, sih]
status: complete
date: 2026-09-25
---

# Evaluation MOC — metrics, suites, experiments, evidence

> Everything this repo uses (and still needs) to turn a run into a graded claim: the 8 metric
> notes, the benchmark suites they run on, the six missing experiments E1–E6, the evidence files
> that exist today, and the R1–R20 scorecard. Nothing here is a claim without an artifact — the
> rule is stated in [[16-testing-evaluation-strategy]] §16.4.

## 1. Metrics (`docs/research/metrics/`, 8 notes)

| Metric | What it answers | Formula (brief) | Where it is mandated |
|---|---|---|---|
| [[Geometric Mean Runtime]] | "how many times slower than the baseline?" | `GM = exp((1/n)·Σ ln t_i)`; Dolan–Moré profile behind it | R16, R20; E1 |
| [[Relative Optimality Gap]] | solution quality, UB *and* LB | `g = (UB − LB)/max(\|UB\|, ε)` | R20; every compare table row |
| [[KKT Residual]] | is the answer *certifiably* correct? | max of stationarity / primal / dual / complementarity, normalized | R9, R17; [[IndependentVerifiers]] |
| [[Primal Residual]] | feasibility violation only | `max\|Ax − b\|` (scaled) | unit + robustness tests |
| [[Numerical Error]] | accuracy lost to rounding | backward error vs refinement delta | R9; E3 |
| [[Cut Efficiency]] | did cuts buy node reduction? | `1 − N(cuts on)/N(cuts off)` | R5; E6 |
| [[Parallel Speedup]] | does more help? | `S_p = T_1/T_p` | R7; E5 |
| [[Parallel Efficiency]] | same, normalized | `E_p = S_p/p` (today: 0.56× → 14 %) | R7; E5 |

**Rule:** a number may appear in README/STATUS/deck/video only if one of these eight produced it
*and* the artifact + hardware are named ([[16-testing-evaluation-strategy]] §16.4).

## 2. Benchmark suites

| Suite | Data | Runner / ctest | Status |
|---|---|---|---|
| Netlib LP | `data/netlib/` (17) | `netlib_benchmarks` → `scripts/run_netlib.py` | working; 7 + 5 extended results |
| MIPLIB | `data/miplib/` (3) | `miplib_benchmarks` → `scripts/run_miplib.py` | working but thin |
| Mittelmann | *absent* | `mittelmann_benchmarks` (planned) | not built (P1-6) |
| QPLIB | `data/qp/` (**empty**) | `qplib_benchmarks` (planned) | not built (P1-6) |
| GPU crossover | `data/scale_study/` | `gpu_benchmarks` → `scripts/run_gpu.py` | **status-check bug** at `run_gpu.py:241-245` |
| Robustness (E3) | `data/robustness/` (planned) | `robustness_suite` (planned) | not built (P0-5) |
| Comparison vs baseline | `data/compare/` (planned) | `compare_harness` → `scripts/run_compare.py` | **does not exist** (P0-1, R16) |

Wiring detail: [[benchmark-suites]] · [[testing-gaps]] · [[Datasets MOC]].

## 3. Missing experiments E1–E6 (from [[09-research-code-alignment]] §6.6)

| # | Experiment | Validates | Blocked by |
|---|---|---|---|
| E1 | head-to-head vs HiGHS: time + quality, geometric mean, Dolan–Moré profile | R16, R20 | harness (none exists) |
| E2 | Mittelmann-style LP/MILP suite run | R15 | instance download + harness |
| E3 | hard-instance dossier: degenerate / ill-conditioned / weak-relaxation | R17, R13 | instance curation + iterative refinement |
| E4 | GPU vs CPU per-scale with recorded hardware + fixed `run_gpu.py` status check | R8 | bug fix (K4) |
| E5 | parallel scaling curve 1/2/4/8 threads after the work-stealing fix | R7 | AP-4 / RW-2 |
| E6 | cut-node reduction after the in-tree cut loop | R5 | AP-3 / RW-1 |

Owning work items: E1/E4/E5/E6 → [[15-roadmap]] P0-1…P0-4; E3 → P0-5; E2 → P1-6.

## 4. Evidence files

Full table with instance counts, hardware columns and supported R numbers: [[04-evidence-inventory]].

- **Results:** `evidence/netlib_results.csv` · `evidence/netlib_extended.csv` · `evidence/miplib_results.csv`.
- **Benchmarks:** `evidence/benchmarks/{phase4.json, crossover_study.csv, crossover_plot.svg, gpu_profile_summary.json, nsight_profile_analysis.md}`.
- **Verification:** `evidence/local-verification-report.txt` (41/41, not 43) · `evidence/environment-local.json` (no CPU/GPU).
- **Missing entirely:** `evidence/compare/` (E1), `evidence/robustness*` (E3), `evidence/hardware.md` (step 0), Mittelmann/QPLIB results.
- **Counter-evidence we must not hide:** GPU loses 13/13 end-to-end; 4 threads = 0.56×; root cuts = 0.0 % node reduction.

## 5. Current scorecard — R1–R20 (copied from [[09-research-code-alignment]] §6.1)

| R# | Status | R# | Status | R# | Status | R# | Status |
|---|---|---|---|---|---|---|---|
| R1 | GOOD | R6 | PARTIAL | R11 | PARTIAL | R16 | **GAP** |
| R2 | GOOD | R7 | **REGRESSION** | R12 | PARTIAL | R17 | **GAP** |
| R3 | PARTIAL | R8 | **UNPROVEN** | R13 | PARTIAL | R18 | PARTIAL |
| R4 | **GAP** | R9 | PARTIAL | R14 | PARTIAL | R19 | PARTIAL |
| R5 | PARTIAL | R10 | GOOD | R15 | PARTIAL | R20 | **UNPROVEN** |

Rollup: **3 GOOD · 11 PARTIAL · 3 GAP (R4, R16, R17) · 1 REGRESSION (R7) · 2 UNPROVEN (R8, R20)** —
the table is authoritative; §6.7 prose of [[09-research-code-alignment]] miscounts it (see [[21-traceability]] §21.6).
Six P0 items follow from this: R4, R5, R7, R8, R16, R17 (+ R20 via R16).

## 6. How evaluation becomes demonstrable

- **Test plan:** [[16-testing-evaluation-strategy]] — layer-by-layer strategy, the 13 concrete new tests, metric definitions, CI changes, and a requirement→proof DoD table.
- **Demo plan:** [[17-sih-demo-strategy]] — which of these artifacts is shown live at which minute, and the fallback when it fails.
- **Order of work:** [[13-restart-point]] (harness first) → [[15-roadmap]] P0-1…P0-6 → [[18-risk-register]] K1/K2 (the two evaluation risks).
- **Why no baseline today:** [[No External Baseline]] · [[Missing External Baseline Comparison]] · [[no-external-baseline]].

## Related

[[Architecture MOC]] · [[Algorithms MOC]] · [[Datasets MOC]] · [[Research MOC]] · [[Codebase MOC]] · [[Research-Code Traceability MOC]] · [[14-target-architecture]] (evaluation layer = *NEW*)
