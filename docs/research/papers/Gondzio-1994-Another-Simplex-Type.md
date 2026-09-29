---
type: paper
title: "Another Simplex-Type Method for Large Scale Linear Programming"
authors: "Gondzio"
year: 1994
venue: "(unverified)"
doi: "(unverified)"
domain: [lp, sparse]
priority: ○
status: standard
tags: [paper, lp, sparse]
---

# Another Simplex-Type Method for Large Scale Linear Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Dual simplex paired with hypersparse linear algebra so large-scale LP iterations stay proportional to touched entries.
## Metadata
| Field | Value |
|---|---|
| Authors | Gondzio |
| Year | 1994 |
| Venue | (unverified) |
| DOI/URL | (unverified) |
## Problem Addressed
On very large LPs the simplex bottleneck is not pivot choice but the linear algebra: dense-style solves over columns/rows that the iteration never touches. The paper rebuilds a simplex-type method around hypersparse solves so per-iteration cost scales with the nonzeros actually involved.
## Core Contribution
- **Methodology:** Dual simplex variant whose FTRAN/BTRAN operations exploit extreme sparsity of the right-hand side (single-nonzero or few-nonzero patterns), avoiding work proportional to model dimension (approximate).
- **Assumptions:** LPs with sparse pricing/ratio right-hand sides; factorization available (approximate).
- **Benchmarks/datasets:** Large-scale LPs of the early 1990s (qualitative; no numbers asserted).
- **Metrics:** Time per iteration, flops per pivot (qualitative).
- **Key results:** Per-iteration cost drops from O(n)-style scans toward O(nnz touched), which decides feasibility at large scale (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Dual simplex with hypersparse triangular solves.
**Techniques:** Hyper-sparsity exploitation in FTRAN/BTRAN, sparse row/column workspaces, avoidance of dense accumulation arrays.
**Implementation details:** For our src/lp/dual/dual_simplex.cpp and src/linalg/sparse_basis.cpp: never allocate full-length dense vectors for a solve whose RHS has one nonzero — carry index lists instead; this is the sparse counterpart to Hall & McKinnon's measurement of the same effect.
**Equations/rules:** Cost of solving Lx = eᵢ is proportional to nnz of the column of L reached from i, not to n (Sparsity).
**Limitations/failure cases:** Hypersparse tricks collapse when RHS patterns are dense (dense columns/rows); bookkeeping errors cause subtle index bugs (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R12's million-variable models make O(n)-per-iteration overhead fatal (R20), and R6 requires sparse technique throughout — but the win must be demonstrated on our benchmark set (R15/R16).
## Evidence → Engineering Decision
- *Finding:* Iteration cost should scale with touched nonzeros, not model size → *PS requirement:* R12 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Hall-2005-Hyper-sparsity-Revised-Simplex]] [[Koberstein-2005-Dual-Simplex-Method]] [[Gilbert-1988-Sparse-Partial-Pivoting]]
## Uses
- [[Dual Simplex]] [[Sparsity]]
