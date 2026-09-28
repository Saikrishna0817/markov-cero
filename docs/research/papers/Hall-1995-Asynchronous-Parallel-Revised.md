---
type: paper
title: "An Asynchronous Parallel Revised Simplex Algorithm"
authors: "Hall & McKinnon"
year: 1995
venue: "(unverified)"
doi: "(unverified)"
domain: [parallel]
priority: ✦
status: standard
tags: [paper, parallel]
---
# An Asynchronous Parallel Revised Simplex Algorithm
> Parallelizes *inside* the revised simplex (FTRAN/BTRAN/PRICE) with asynchronous execution — an alternative axis to tree-level parallelism.
## Metadata
| Field | Value |
|---|---|
| Authors | Hall & McKinnon |
| Year | 1995 |
| Venue | (unverified) |
| DOI/URL | https://www.maths.ed.ac.uk/hall/MS-95-050/ |

## Problem Addressed
Most parallel-MIP effort splits the tree, but each node LP stays serial. This paper asks whether the revised simplex's own kernels — sparse triangular solves and pricing — can run concurrently on MIMD hardware with fast interconnects without breaking numerical behaviour.
## Core Contribution
- **Methodology:** Asynchronous parallel revised simplex: workers perform FTRAN/BTRAN/PRICE tasks without global barriers, tolerating latency; relies on hyper-sparsity so tasks touch few nonzeros.
- **Assumptions:** Fast low-latency interconnect; hyper-sparse LPs where most rows/columns are tiny; tolerable staleness of intermediate results.
- **Benchmarks/datasets:** LP test problems of the Edinburgh group (Netlib-adjacent; not re-verified).
- **Metrics:** Iteration time, speedup, effect on iteration count/numerics.
- **Key results:** Meaningful per-iteration speedups on large hyper-sparse LPs; barrier-free execution hides communication latency (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex with asynchronous FTRAN/BTRAN/PRICE.
**Techniques:** Exploit hyper-sparsity (small touched sets) so tasks are fine-grained; asynchrony instead of barriers to hide latency.
**Implementation details:** Our `src/lp/reference/revised_simplex.cpp` and `src/linalg/sparse_basis.cpp` are strictly sequential; this is a per-node option if tree parallelism stays unprofitable — but it only pays off on LPs much larger than our current test set.
**Limitations/failure cases:** Requires large hyper-sparse LPs and low-latency hardware; on tiny instances (AFIRO, ADLITTLE) coordination would dominate.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R7 wants multi-core speedup; if tree-level parallelism stays at 0.56×, intra-LP parallelism is the documented fallback for large instances (R12).
## Evidence → Engineering Decision
- *Finding:* Parallelism currently sits only at tree level while node LPs run serial (phase4.json: 0.56× overall) → *PS requirement:* R7 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* per-node LP time vs. threads
## Related Papers
- [[Anderson-1989-Solving-Sparse-Linear]]
- [[Berthold-2019-Parallel-SCIP-UG]]
## Uses
- [[Sparsity]]
