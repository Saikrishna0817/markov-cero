---
type: paper
title: "NETLIB LP Test Set; Solving Linear Systems on Backward Stable Computers"
authors: "Anderssen & Klee; Anderson & Wright (as merged in list)"
year: 1984
venue: "(unverified)"
doi: "(unverified)"
domain: [benchmark]
priority: ★
status: deep
tags: [paper, benchmark]
---

# NETLIB LP Test Set; Solving Linear Systems on Backward Stable Computers

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The classic Netlib LP collection (AFIRO, E226, PILOT, DFL001 …) — the correctness and performance substrate every LP solver is judged on, paired here with a backward-stability reference for the linear algebra underneath.

## Metadata
| Field | Value |
|---|---|
| Authors | Anderssen & Klee (1984) — entry also cites Anderson & Wright, "Solving Linear Systems on Backward Stable Computers" |
| Year | 1984 |
| Venue | (unverified) |
| DOI/URL | (unverified); instances live at netlib.org / Netlib `lp/data` |

## Problem Addressed
LP solver claims need instances everyone can obtain, with known optimal objectives, spanning tiny degenerate models through large sparse industrial ones. The entry documents the Netlib LP test set (115+ classic LPs) plus the numerical-linear-algebra stability criteria used to evaluate the solvers that process them.

## Core Contribution
- **Methodology:** Curated free LP collection distributed by Netlib with reference objective values, spanning tiny models (AFIRO 28×32) to large sparse ones (E226, PILOT, DFL001); paired with backward-stability criteria for linear system solving.
- **Assumptions:** Instances are solvable in double precision to the published objectives; formats parseable as free/fixed MPS.
- **Benchmarks/datasets:** Netlib LP set — our runs cover 12 instances: AFIRO, ADLITTLE, SC50A, SC50B, SC105, SHARE2B, RECIPE (netlib_results.csv) + SC205, SHARE1B, SCAGR7, BEACONFD, SCORPION (netlib_extended.csv).
- **Metrics:** Objective agreement vs. reference, primal/dual violation, runtime, iteration counts.
- **Key results:** Our engine matches reference objectives to ~1e-15 relative error on the covered instances; coverage stops far short of the full collection (numbers from evidence CSVs).

## Engineering-Relevant Knowledge
**Algorithms:** Regression benchmarking of LP solvers on fixed reference objectives.
**Techniques:** Backward-stable solves (normwise error bounds) as the criterion for the linear algebra layer; exact reference objectives as ground truth for a zero-trust verifier.
**Implementation details:** `src/io/mps.cpp` parses these files; `evidence/netlib_results.csv` and `netlib_extended.csv` hold our current results; the remaining Netlib instances are the cheapest path to broader R15 coverage.
**Equations/rules:** Relative objective error = |f − f_ref| / max(1, |f_ref|); pass only if error and primal/dual violations are both under tolerance.
**Limitations/failure cases:** Many classic Netlib LPs are degenerate or ill-conditioned — the known failure surface of naive simplex (our BLEND NumericalFailure is the same family).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R15/R19 name the Netlib LP set explicitly; it is our only benchmark family with meaningful breadth today and the natural home for R17 robustness demonstrations.

## Evidence → Engineering Decision
- *Finding:* 12 Netlib instances solved with verified objectives vs. 115+ available (evidence/netlib_results.csv, netlib_extended.csv) → *PS requirement:* R15 → *Component:* src/io/mps.cpp → *Metric:* instances verified / collection size
- *Finding:* BLEND returns NumericalFailure in our simplex while CPU/GPU PDLP reach the optimum (_deployment-phase2-build/gpu_benchmark.csv) → *PS requirement:* R17 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* [[Relative Optimality Gap]] on degenerate Netlib instances

## Related Papers
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Mittelmann-n.d.-Mittelmann-LP-MILP]]
- [[Lodi-2013-Performance-Variability-Mixed]]

## Uses
- [[Netlib LP Collection]]
