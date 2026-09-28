---
type: paper
title: "Accelerating Optimization Solvers on GPUs"
authors: "(authors not stated in source list)"
year: n.d.
venue: "HAL / CNRS repository"
doi: "(unverified)"
domain: [gpu]
priority: ✦
status: standard
tags: [paper, gpu]
---
# Accelerating Optimization Solvers on GPUs
> Survey of GPU acceleration beyond first-order methods — second-order/interior-point kernels and vendor sparse direct libraries (e.g. NVIDIA cuDSS for sparse LU/Cholesky).
## Metadata
| Field | Value |
|---|---|
| Authors | Not stated in the source list (verify: HAL/CNRS record) |
| Year | n.d. (no year in source list) |
| Venue | HAL / CNRS repository |
| DOI/URL | https://hal.cnrs.fr/ (record not pinned in source list) |

## Problem Addressed
First-order methods are not the only way to use a GPU: factorization-based solvers (IPM, SQP) depend on sparse LU/Cholesky, which vendors now ship as GPU libraries. The item asks what is available and what it costs to integrate.
## Core Contribution
- **Methodology:** Survey of GPU techniques for optimization solvers: sparse direct factorizations on GPU, batched linear algebra for second-order methods, and kernel-level acceleration of barrier/Newton steps.
- **Assumptions:** Use of vendor libraries (cuDSS/cuSolver) rather than from-scratch factorization; large sparse models.
- **Benchmarks/datasets:** Reported vendor and research benchmarks (not re-verified).
- **Metrics:** Factorization/iteration throughput, end-to-end solver time, transfer overhead.
- **Key results:** GPU sparse direct libraries make factorization-heavy methods competitive at scale; integration overhead dominates on small problems (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** [[Interior-Point Method]] Newton systems on GPU; sparse LU/Cholesky via vendor libraries.
**Techniques:** Keep matrix resident on device; batch repeated factorizations; avoid host-device round trips per iteration.
**Implementation details:** R10 requires building the solver from mathematical foundations, so cuDSS-style libraries are reference/performance-boundary material, not a component we can lean on; our `src/linalg/sparse_basis.cpp` stays ours.
**Limitations/failure cases:** Vendor libraries add external dependency and licensing questions; useless below the crossover size where transfers dominate.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Informs the future IPM/QP engine (R2/R4) and the honest statement of what a sovereign from-scratch solver does and does not implement (R10/R18).
## Evidence → Engineering Decision
- *Finding:* Our only GPU path today is first-order PDHG; no GPU factorization exists in-tree → *PS requirement:* R4 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* factorization time per IPM iteration (roadmap metric)
## Related Papers
- [[Unknown-2025-Overview-GPU-Based-First]]
- [[Amestoy-2000-Parallel-Sparse-Linear]]
## Uses
- [[GPU CSR SpMV]]
