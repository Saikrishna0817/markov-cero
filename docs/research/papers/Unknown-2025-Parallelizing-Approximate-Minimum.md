---
type: paper
title: "Parallelizing the Approximate Minimum Degree Ordering Algorithm"
authors: "(authors not stated in source list)"
year: 2025
venue: "arXiv"
doi: "(unverified)"
domain: [parallel]
priority: ○
status: standard
tags: [paper, parallel]
---
# Parallelizing the Approximate Minimum Degree Ordering Algorithm

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> First scalable shared-memory parallel AMD ordering — reported 7.29× on 64 threads — removing a serial bottleneck before any factorization starts.
## Metadata
| Field | Value |
|---|---|
| Authors | Not stated in the source list (verify: arXiv 2504.17097) |
| Year | 2025 |
| Venue | arXiv |
| DOI/URL | https://arxiv.org/html/2504.17097v2 |

## Problem Addressed
Fill-reducing orderings (AMD/COLAMD) are computed serially and can dominate setup time for large sparse factorizations, delaying every parallel solve that follows. The paper addresses making AMD itself scale on shared-memory machines.
## Core Contribution
- **Methodology:** Parallelizes AMD's degree updates, elimination-tree maintenance and supervariable amalgamation with lock-free/finer-grained updates over the quotient graph.
- **Assumptions:** Shared memory with many cores; ordering quality must match serial AMD (no fill regression).
- **Benchmarks/datasets:** Large sparse matrices from sparse-direct collections (not re-verified).
- **Metrics:** Ordering wall-clock time, speedup, resulting fill (nnz of L) vs. serial AMD.
- **Key results:** 7.29× speedup on 64 threads with ordering quality preserved (as stated in the source list).
## Engineering-Relevant Knowledge
**Algorithms:** Approximate Minimum Degree on the quotient graph; parallel degree updates and supervariable detection.
**Techniques:** Coarse parallelism over independent degree clusters; avoid global minimum search every step by batched selection.
**Implementation details:** Ordering is done once per refactorization in an IPM/KKT path (`src/linalg/sparse_basis.cpp`); for simplex basis updates the structure changes every pivot, so per-pivot ordering must stay cheap or be skipped.
**Limitations/failure cases:** Speedup falls for small/ill-conditioned graphs with tiny quotient graphs — same regime as our Netlib-sized instances.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Only becomes relevant when instance scale (R12) makes ordering a measurable part of runtime; today our instances are small enough that ordering is noise.
## Evidence → Engineering Decision
- *Finding:* No ordering-time telemetry exists in evidence/benchmarks CSVs → *PS requirement:* R12 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* ordering time as a share of total solve time
## Related Papers
- [[Amestoy-2000-Parallel-Sparse-Linear]]
- [[Anderson-1989-Solving-Sparse-Linear]]
## Uses
- [[Sparsity]]
