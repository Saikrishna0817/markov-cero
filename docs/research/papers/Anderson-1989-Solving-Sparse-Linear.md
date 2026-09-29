---
type: paper
title: "Solving Sparse Linear Systems on a Hypercube; Updating LU Factors (Hager)"
authors: "Anderson & Saad; Hager"
year: 1989
venue: "(unverified)"
doi: "(unverified)"
domain: [parallel]
priority: ○
status: standard
tags: [paper, parallel]
---
# Solving Sparse Linear Systems on a Hypercube; Updating LU Factors (Hager)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Parallel triangular solves and LU-update alternatives that attack the sequential core of every simplex FTRAN/BTRAN.
## Metadata
| Field | Value |
|---|---|
| Authors | Anderson & Saad (1989); Hager (1980) |
| Year | 1989 (first entry) |
| Venue | (unverified) |
| DOI/URL | (unverified) |

## Problem Addressed
Forward/backward substitution on a triangular factor is inherently sequential: each step depends on the previous one. On parallel hardware this serial chain caps speedup (Amdahl), so the paper explores parallel hypercube solves and alternative formulations based on updating LU factors rather than substituting.
## Core Contribution
- **Methodology:** Parallel sparse triangular solution algorithms (level scheduling / graph-based parallelism across independent rows) plus Hager-style LU updating methods that trade substitution for matrix products.
- **Assumptions:** Sparse factor with moderate fill; enough independent row levels to expose parallelism.
- **Benchmarks/datasets:** Sparse test matrices of the era (not re-verified).
- **Metrics:** Solve time and speedup vs. serial substitution.
- **Key results:** Level-scheduled triangular solves give partial speedup proportional to the number of independent levels; deeper trees limit it (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithm:** FTRAN/BTRAN sparse triangular solves (Gilbert–Peierls-style) with level scheduling from the elimination tree.
**Techniques:** Parallelize across independent rows within a level; consider refactorization instead of many small updates when the basis churns.
**Implementation details:** Our `src/linalg/sparse_basis.cpp` does sequential FTRAN/BTRAN; level arrays would be cheap to compute but instance sizes today are tiny.
**Limitations/failure cases:** Highly triangular/deep elimination trees (common in degenerate LP bases) yield almost no independent levels — parallel FTRAN then buys nothing.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** If tree-level parallelism stays at 0.56×, intra-LP parallelism (R7 + R6) is the alternative axis; this paper says where the remaining serial fraction lives.
## Evidence → Engineering Decision
- *Finding:* 4-thread end-to-end efficiency is 14.1% (evidence/benchmarks/phase4.json) while per-node LP work is entirely serial → *PS requirement:* R7 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* serial fraction per node vs. [[Parallel Efficiency]]
## Related Papers
- [[Amestoy-2000-Parallel-Sparse-Linear]]
- [[Hall-1995-Asynchronous-Parallel-Revised]]
## Uses
- [[Sparse LU]]
