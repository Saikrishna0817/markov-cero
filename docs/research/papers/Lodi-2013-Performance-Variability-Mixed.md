---
type: paper
title: "Performance Variability in Mixed-Integer Programming"
authors: "Lodi & Tramontani"
year: 2013
venue: "TutORials in Operations Research"
doi: "10.1287/educ.2013.0112"
domain: [benchmark]
priority: ★
status: deep
tags: [paper, benchmark]
---

# Performance Variability in Mixed-Integer Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Shows that MIP runtimes vary by orders of magnitude under equivalent reformulations and random seeds — single-run benchmark tables are not evidence.

## Metadata
| Field | Value |
|---|---|
| Authors | Lodi & Tramontani |
| Year | 2013 |
| Venue | TutORials in Operations Research |
| DOI/URL | 10.1287/educ.2013.0112 |

## Problem Addressed
Two runs of the same solver on the same model can differ dramatically once variables are permuted, coefficients rescaled, or the seed changed — even though the mathematical problem is identical. The paper addresses what this variability implies for experimental design in MIP papers.

## Core Contribution
- **Methodology:** Collect many runs under randomized seeds and equivalent model permutations; characterize the resulting runtime distributions (heavy tails, wide dispersion); derive protocol recommendations for reporting.
- **Assumptions:** Reformulations are mathematically equivalent; randomness enters via tie-breaking, heuristics and exploration order.
- **Benchmarks/datasets:** MIPLIB-derived instances subjected to random variable/constraint orderings and multiple seeds (exact counts not re-verified).
- **Metrics:** Distribution of runtimes (quantiles, tails), dispersion across permutations, probability of solving within a limit.
- **Key results:** Single-run rankings of solvers are unstable; multiple runs with dispersion reporting are mandatory for credible comparison (qualitative restatement; figures not re-verified).

## Engineering-Relevant Knowledge
**Algorithms:** Experimental design for MIP evaluation; random permutation/seed protocols.
**Techniques:** N ≥ several runs per instance; report median/quantiles or "solved within limit in k of N runs" rather than one wall-clock number.
**Implementation details:** Every number in `evidence/benchmarks/phase4.json` and the Netlib/MIPLIB CSVs is a single run with no dispersion — including the headline 0.56× speedup, which is therefore fragile evidence.
**Equations/rules:** Report P(solve within T) over runs; never rank solvers by one run's runtime when distributions overlap.
**Limitations/failure cases:** Multiplies benchmark cost by N; needs a harness that supports seeds/permutations — absent today.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R16/R20 ask for comparison against an established solver; this paper says our entire comparison protocol (and our own parallel-speedup claim) is invalid until runs are repeated and dispersion disclosed.

## Evidence → Engineering Decision
- *Finding:* phase4.json reports speedup_4th 0.56× from a single 4-thread timing on one instance → *PS requirement:* R15 → *Component:* benchmarks/runners → *Metric:* [[Parallel Speedup]] median and quantiles over ≥5 repeated runs

## Related Papers
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]
- [[Unknown-2024-Distributional-MIPLIB]]
- [[Schweizer-2018-Deterministic-Parallel-MIP]]

## Uses
- [[Geometric Mean Runtime]]
