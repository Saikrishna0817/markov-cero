---
type: paper
title: "Parallel Symbolic Factorization for Sparse LU with Static Pivoting"
authors: "Grigori, Demmel & Li"
year: 2007
venue: "SIAM J. Sci. Comput."
doi: "10.1137/050638102"
domain: [sparse]
priority: ✦
status: standard
tags: [paper, sparse]
---

# Parallel Symbolic Factorization for Sparse LU with Static Pivoting
> Memory-scalable parallel symbolic analysis for sparse LU, with static pivoting to avoid parallel pivot communication.
## Metadata
| Field | Value |
|---|---|
| Authors | Grigori, Demmel & Li |
| Year | 2007 |
| Venue | SIAM J. Sci. Comput. |
| DOI/URL | 10.1137/050638102 |
## Problem Addressed
Parallel sparse LU pays twice: the symbolic analysis (elimination DAG, column counts, partitioning) was itself a serial/memory bottleneck, and dynamic partial pivoting forces communication. The paper parallelizes the symbolic phase and pairs it with static pivoting so the numeric phase stays data-parallel.
## Core Contribution
- **Methodology:** Graph-partitioned parallel symbolic factorization computing column counts and elimination structure with bounded memory, combined with static (threshold-free) pivoting to eliminate pivot synchronization (approximate).
- **Assumptions:** Sparsity pattern available ahead of time; static pivoting judged numerically acceptable with iterative refinement as a safety net (approximate).
- **Benchmarks/datasets:** Large unsymmetric sparse matrices from real applications (qualitative; no figures asserted).
- **Metrics:** Symbolic analysis time, memory footprint, numeric factorization scalability (qualitative).
- **Key results:** Symbolic phase becomes scalable and memory-bounded; overall LU pipeline runs with far less synchronization (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Parallel symbolic analysis for sparse LU; static pivoting with refinement-based correction.
**Techniques:** Symbolic Factorization, graph partitioning for load balance, static pivoting, iterative refinement repair.
**Implementation details:** The static-pivoting + refinement pattern is the standard accelerator-friendly recipe — directly relevant to R8 GPU questions: avoid parallel dynamic pivoting, then fix accuracy with refinement rather than communication.
**Equations/rules:** Compute column counts via the symbolic DAG; if static pivot p is small, correct the solve via iterative refinement (Numerical Error).
**Limitations/failure cases:** Static pivoting can fail on genuinely indefinite/ill-conditioned matrices without refinement; graph partitioning overhead matters only at scale (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Points at a real design fork for R7/R8/R9: dynamic pivoting (our current LP-basis route) vs. static pivoting + refinement (accelerator-friendly); which wins for our models is an experiment, not a citation.
## Evidence → Engineering Decision
- *Finding:* Static pivoting plus refinement can replace dynamic pivoting on parallel hardware → *PS requirement:* R8 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* KKT Residual
## Related Papers
- [[Ribizel-2023-Parallel-Symbolic-Cholesky]] [[Suhl-1990-Fast-LU-Factorization]] [[Markowitz-1957-Elimination-Form-Inverse]]
## Uses
- [[Symbolic Factorization]] [[Iterative Refinement]]
