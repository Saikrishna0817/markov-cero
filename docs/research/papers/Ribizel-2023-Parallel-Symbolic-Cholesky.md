---
type: paper
title: "Parallel Symbolic Cholesky Factorization"
authors: "Ribizel & Anzt"
year: 2023
venue: "SC-W"
doi: "10.1145/3624062.3624253"
domain: [sparse]
priority: ✦
status: standard
tags: [paper, sparse]
---

# Parallel Symbolic Cholesky Factorization

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Symbolic factorization executed in parallel on CPU/GPU — the setup phase for large-scale IPM/QP factorizations.
## Metadata
| Field | Value |
|---|---|
| Authors | Ribizel & Anzt |
| Year | 2023 |
| Venue | SC-W |
| DOI/URL | 10.1145/3624062.3624253 |
## Problem Addressed
Interior-point and QP solvers factor many matrices with the same sparsity pattern; the symbolic phase (elimination tree, column counts, fill prediction) must not become a serial bottleneck, especially on accelerator hardware where the numeric phase already runs fast.
## Core Contribution
- **Methodology:** Parallel symbolic Cholesky analysis designed for GPU/CPU execution, computing elimination-tree-based structure without sequential bottlenecks (approximate).
- **Assumptions:** Symmetric positive-definite normal-equation-like structures; pattern reused across numeric factorizations (approximate).
- **Benchmarks/datasets:** Large sparse SPD problems (qualitative; no figures asserted).
- **Metrics:** Symbolic-analysis time, scaling with threads/threads-per-block, memory use (qualitative).
- **Key results:** Symbolic factorization can be made to scale in parallel rather than being an irreducible serial overhead (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Parallel symbolic factorization feeding numeric Cholesky/LDLᵀ.
**Techniques:** Symbolic Factorization with parallel tree traversal, structure reuse across refactorizations, accelerator-aware data structures.
**Implementation details:** Relevant to R8: any GPU numeric kernel for IPM/QP normal equations is only as fast as its setup; our current CPU code should first make symbolic analysis reusable (once per model), then measure whether parallelizing it matters.
**Equations/rules:** Symbolic phase computes colcount(j) = |row subtree of j| without values; numeric phase can then skip all pattern discovery.
**Limitations/failure cases:** Cholesky only (SPD); indefinite KKT systems used by IPM need LDLᵀ variants (approximate); benefits vanish if the pattern changes every iteration.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R7 (multi-core) and R8 (GPU where measurable) both point at separating symbolic from numeric work — first for reuse on CPU, later as the entry point for GPU effort where measurement justifies it.
## Evidence → Engineering Decision
- *Finding:* Reusable symbolic analysis removes repeated setup cost across refactorizations → *PS requirement:* R7 → *Component:* src/linalg/ → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Grigori-2007-Parallel-Symbolic-Factorization]] [[Davis-2004-Column-Approximate-Minimum]] [[Liu-1990-Role-Elimination-Trees]]
## Uses
- [[Symbolic Factorization]] [[Fill-Reducing Ordering]]
