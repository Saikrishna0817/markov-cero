---
type: paper
title: "Parallelizing the Dual Revised Simplex Method"
authors: "Huangfu & Hall"
year: 2018
venue: "Math. Prog. Comp."
doi: "(unverified)"
domain: [lp]
priority: ✦
status: standard
tags: [paper, lp]
---

# Parallelizing the Dual Revised Simplex Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> The algorithmic basis of the HiGHS simplex: how to put parallelism inside the dual revised simplex without breaking it.
## Metadata
| Field | Value |
|---|---|
| Authors | Huangfu & Hall |
| Year | 2018 |
| Venue | Math. Prog. Comp. |
| DOI/URL | https://link.springer.com/article/10.1007/s12532-017-0130-5 |
## Problem Addressed
The simplex has a reputation as inherently sequential — it was long assumed parallelism could not pay; the paper identifies the parallelizable parts (factorization, sparse solves, pricing/ratio scans) and restructures the dual revised simplex so multi-core execution is real rather than theoretical.
## Core Contribution
- **Methodology:** Redesign of the dual revised simplex around parallel sparse linear algebra (factorization/triangular solves) and parallel pricing, with careful attention to the sequential critical path (approximate).
- **Assumptions:** Multi-core shared memory; sparsity preserved enough for parallel sparse kernels (approximate).
- **Benchmarks/datasets:** Netlib/LP test sets used for HiGHS development (approximate; no numbers asserted).
- **Metrics:** Runtime, parallel efficiency, iteration counts (qualitative).
- **Key results:** Demonstrates that a dual simplex can be made to scale on multi-core hardware — the design behind HiGHS' simplex (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Parallel dual revised simplex with refactorization and parallel pricing.
**Techniques:** Parallel sparse triangular solves, parallel ratio test, parallel factorization, work partitioning over candidate columns.
**Implementation details:** Directly relevant to R7: our src/lp/dual/dual_simplex.cpp is currently sequential, and this tells us where threads belong (pricing scan, factorization, solves) and where they cannot (pivot choice serial dependency).
**Equations/rules:** Serial path = choose pivot; parallel region = compute all candidate columns/values (cost O(nnz) over many columns) (Sparsity).
**Limitations/failure cases:** Speedups depend on model sparsity and size — small LPs get no benefit; load imbalance on irregular patterns (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R7 requires multi-core parallelization; this shows it must be designed into the simplex algorithm (not bolted on), and gives a realistic expectation of where parallel time exists — measurable against our sequential baseline (R16/R20).
## Evidence → Engineering Decision
- *Finding:* Parallelism belongs in pricing/factorization, not pivot selection → *PS requirement:* R7 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Koberstein-2005-Dual-Simplex-Method]] [[Hall-2005-Hyper-sparsity-Revised-Simplex]] [[Gondzio-1994-Another-Simplex-Type]]
## Uses
- [[Dual Simplex]] [[Sparsity]] [[Revised Simplex]]
