---
type: paper
title: "Sympiler: Transforming Sparse Matrix Codes by Decoupling Symbolic Analysis"
authors: "Cheshmi, Kamil, Strout & Mehri Dehnavi"
year: 2017
venue: "SC'17"
doi: "10.1145/3126908.3126936"
domain: [sparse]
priority: ✦
status: standard
tags: [paper, sparse]
---

# Sympiler: Transforming Sparse Matrix Codes by Decoupling Symbolic Analysis

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> A compiler that separates symbolic analysis from numeric kernels so sparse codes can be specialized and optimized once per pattern.
## Metadata
| Field | Value |
|---|---|
| Authors | Cheshmi, Kamil, Strout & Mehri Dehnavi |
| Year | 2017 |
| Venue | SC'17 |
| DOI/URL | 10.1145/3126908.3126936 |
## Problem Addressed
Sparse codes are hand-written repeatedly for every matrix structure because numeric kernels and symbolic decisions are entangled; Sympiler shows how to lift the symbolic part into a compile/codegen step so numeric kernels become structure-specialized and fast.
## Core Contribution
- **Methodology:** Decouples symbolic analysis (loop/index structure derived from the sparsity pattern) from numeric execution, generating specialized kernels via transformation templates (approximate).
- **Assumptions:** Pattern is static or changes rarely; a code generator can specialize per pattern class (approximate).
- **Benchmarks/datasets:** Sparse kernels from scientific computing suites (qualitative; no numbers asserted).
- **Metrics:** Kernel runtime vs. hand-optimized and generic sparse libraries (qualitative).
- **Key results:** Specializing on decoupled symbolic structure yields large kernel speedups without hand-writing every case (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Symbolic-codegen for sparse kernels (factorization/setup loops, SpMV, triangular solve patterns).
**Techniques:** Symbolic/numeric decoupling, pattern-specialized code generation, compile-time optimization of index structure.
**Implementation details:** A realistic idea is limited: our solver can benefit from the *discipline* — keep symbolic passes (pattern, reach, elimination order) separate from numeric ones so they can be cached and reused (src/linalg/) — but full codegen is far beyond scope (R10/R18).
**Equations/rules:** Numeric kernel cost = f(pattern); once the pattern is fixed, index computations are invariant and hoistable.
**Limitations/failure cases:** Patterns that change per iteration (basis updates) reduce specialization value; codegen complexity is high for a from-scratch project (approximate).
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Supports the architectural rule that symbolic analysis must live behind its own interface (R18 transparent/extensible, R6 sparse) — good justification for caching symbolic state, not a component we should build.
## Evidence → Engineering Decision
- *Finding:* Separating symbolic from numeric phases enables caching and specialization → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Grigori-2007-Parallel-Symbolic-Factorization]] [[Ribizel-2023-Parallel-Symbolic-Cholesky]] [[Amestoy-1996-Approximate-Minimum-Degree]]
## Uses
- [[Symbolic Factorization]] [[Sparsity]]
