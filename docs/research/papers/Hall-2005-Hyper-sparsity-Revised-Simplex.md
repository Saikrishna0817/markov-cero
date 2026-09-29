---
type: paper
title: "Hyper-sparsity in the Revised Simplex Method and How to Exploit It"
authors: "Hall & McKinnon"
year: 2005
venue: "Comp. Optim. Appl."
doi: "(unverified)"
domain: [lp, sparse]
priority: ✦
status: standard
tags: [paper, lp, sparse]
---

# Hyper-sparsity in the Revised Simplex Method and How to Exploit It

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> FTRAN/BTRAN/PRICE exploit hyper-sparsity — reported 5.2× speedup on the operations that dominate each iteration.
## Metadata
| Field | Value |
|---|---|
| Authors | Hall & McKinnon |
| Year | 2005 |
| Venue | Comp. Optim. Appl. |
| DOI/URL | https://link.springer.com/article/10.1007/s10589-005-2404-4 |
## Problem Addressed
In large LPs the right-hand sides of simplex's linear algebra (pricing column, ratio-test column, objective row) are often almost entirely zero — "hyper-sparse" — yet standard implementations still run dense-length loops, so iteration cost scales with model size instead of with the nonzeros actually touched.
## Core Contribution
- **Methodology:** Identifies hyper-sparsity in FTRAN, BTRAN and pricing operations and redesigns each to iterate over nonzero index lists rather than full vectors (approximate).
- **Assumptions:** RHS/outputs genuinely sparse on large models; index-list bookkeeping overhead is small relative to skipped work (approximate).
- **Benchmarks/datasets:** Large LPs including Netlib instances (qualitative; list records a 5.2× speedup).
- **Metrics:** Time per FTRAN/BTRAN/PRICE operation, overall iteration time.
- **Key results:** 5.2× speedup from exploiting hyper-sparsity (as recorded in the reference list).
## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex operations: FTRAN (basis solve), BTRAN (dual solve), pricing scan.
**Techniques:** Sparse index-list traversal, sparse accumulation arrays, avoidance of dense workspace clearing.
**Implementation details:** For src/lp/dual/dual_simplex.cpp and src/linalg/sparse_basis.cpp: store solves as index sets, never memset full-length arrays in the iteration path; the win grows with model size, so this is an R12-scale concern as much as an R6 one.
**Equations/rules:** Cost(FTRAN with k-nonzero RHS) ∝ entries reached, not n (Sparsity).
**Limitations/failure cases:** Benefits vanish when outputs are dense (dense rows/columns); index bookkeeping is error-prone (approximate).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R6 demands sparse technique and R12/R20 demand scale: with our sequential baseline, hyper-sparse operation kernels are the highest-leverage per-iteration optimization identified in the whole simplex literature (measurable via R16).
## Evidence → Engineering Decision
- *Finding:* Hyper-sparse FTRAN/BTRAN/PRICE gives ~5.2× on affected operations → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Gondzio-1994-Another-Simplex-Type]] [[Gilbert-1988-Sparse-Partial-Pivoting]] [[Goldfarb-1977-Practicable-Steepest-Edge]]
## Uses
- [[Sparsity]] [[Revised Simplex]]
