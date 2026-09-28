---
type: paper
title: "Parallel Sparse Linear Algebra in MUMPS"
authors: "Amestoy, Duff & Gu"
year: 2000
venue: "(unverified)"
doi: "(unverified)"
domain: [parallel]
priority: ○
status: standard
tags: [paper, parallel]
---
# Parallel Sparse Linear Algebra in MUMPS
> Task-parallel multifrontal sparse factorization showing how to keep many cores busy inside a single sparse solve.
## Metadata
| Field | Value |
|---|---|
| Authors | Amestoy, Duff & Gu |
| Year | 2000 |
| Venue | (unverified) |
| DOI/URL | (unverified) |

## Problem Addressed
Sparse LU/Cholesky factorization is dominated by irregular, dynamically sized frontal tasks, so static scheduling wastes cores. The paper addresses efficient parallel factorization (and the triangular solves around it) for large sparse systems.
## Core Contribution
- **Methodology:** Multifrontal factorization with dynamic task scheduling of frontal matrices; distributed sparse data structures; asynchronous task execution.
- **Assumptions:** Matrix sparsity is fixed during the solve; tasks are independent once their supernodes complete.
- **Benchmarks/datasets:** Sparse matrices from the Harwell-Boeing/SPARSKIT era (not re-verified).
- **Metrics:** Factorization time, parallel efficiency, memory use.
- **Key results:** Near-linear speedups for factorization of large sparse matrices on moderate core counts (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Multifrontal sparse LU; parallel task scheduling over elimination supernodes.
**Techniques:** Separate symbolic (ordering, structure) from numeric (factor) phase so only numeric work is parallelized.
**Implementation details:** Our `src/linalg/sparse_basis.cpp` factorization is sequential; if we adopt an IPM normal-equations/KKT path, this is the pattern to parallelize it.
**Limitations/failure cases:** Bases change every simplex pivot, so refactorization-heavy simplex gains little from factorizing in parallel; this matters more for interior-point-style solves.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R7 asks for multi-core parallelization; today our parallelism bottleneck is the tree, not the factorization, but any future [[Interior-Point Method]] engine would need this class of parallelism.
## Evidence → Engineering Decision
- *Finding:* Parallel benefit is currently blocked at tree level (0.56×, evidence/benchmarks/phase4.json), not measured inside linear algebra → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* time per factorization vs. thread count
## Related Papers
- [[Anderson-1989-Solving-Sparse-Linear]]
- [[Unknown-2025-Parallelizing-Approximate-Minimum]]
## Uses
- [[Sparse LU]]
