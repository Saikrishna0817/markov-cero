---
type: paper
title: "Distributional MIPLIB (D-MIPLIB)"
authors: "(authors not stated in source list)"
year: 2024
venue: "arXiv"
doi: "(unverified)"
domain: [benchmark]
priority: ✦
status: standard
tags: [paper, benchmark]
---
# Distributional MIPLIB (D-MIPLIB)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Reframes MIPLIB instances as *distributions* of hardness rather than single points — benchmarking that absorbs the performance variability classical instance sets ignore.
## Metadata
| Field | Value |
|---|---|
| Authors | Not stated in the source list (verify: arXiv 2406.12144) |
| Year | 2024 |
| Venue | arXiv |
| DOI/URL | https://arxiv.org/abs/2406.12144 |

## Problem Addressed
A single MIPLIB instance has one runtime, but equivalent perturbations of it (orderings, seeds) form a distribution; ranking solvers on point values is therefore noise-sensitive. The entry addresses capturing instance-level hardness *variation* as the unit of benchmarking.
## Core Contribution
- **Methodology:** Construct a distributional benchmark: sample variants per instance, model the runtime/quality distribution, and evaluate solvers against the distribution rather than a single number.
- **Assumptions:** Variants are mathematically equivalent or controlled perturbations; enough samples per instance to characterize tails.
- **Benchmarks/datasets:** MIPLIB-derived distributional sets (exact construction not re-verified).
- **Metrics:** Probability of solving within a limit, quantiles of runtime, distributional gap quality.
- **Key results:** Distributional evaluation exposes instability hidden by point benchmarks and aligns with the variability literature (details not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Distributional benchmarking of MIP solvers.
**Techniques:** Sample-and-report (quantiles/success probability) instead of single runs; connects seed/permutation protocols to reporting format.
**Implementation details:** Our evidence tables hold one row per instance with one runtime — no distribution — so any claim about "consistently deliver" (R20) is currently unsupported.
**Limitations/failure cases:** Sampling multiplies run cost; needs harness support for seeded runs (absent).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R20 asks for *consistent* delivery of solutions; a distributional view is the honest way to demonstrate consistency once we can afford repeated runs.
## Evidence → Engineering Decision
- *Finding:* Evidence CSVs contain single-run rows only (evidence/benchmarks/*.csv) → *PS requirement:* R20 → *Component:* benchmarks/runners → *Metric:* solve probability within limit over sampled variants
## Related Papers
- [[Lodi-2013-Performance-Variability-Mixed]]
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
## Uses
- [[MIPLIB]]
