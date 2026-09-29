---
type: paper
title: "Prestructuring Sparse Matrices with Dense Rows and Columns via Null Space Methods"
authors: "Howell"
year: 2018
venue: "Numer. Lin. Alg. Appl."
doi: "10.1002/nla.2133"
domain: [sparse]
priority: ✦
status: standard
tags: [paper, sparse]
---

# Prestructuring Sparse Matrices with Dense Rows and Columns via Null Space Methods

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Handles the dense rows/columns that otherwise destroy sparsity in industrial models.
## Metadata
| Field | Value |
|---|---|
| Authors | Howell |
| Year | 2018 |
| Venue | Numer. Lin. Alg. Appl. |
| DOI/URL | 10.1002/nla.2133 |
## Problem Addressed
Real industrial matrices are rarely uniformly sparse: a few dense rows or columns (linking/balance constraints, aggregations) can dominate factorization cost and ruin every sparsity assumption. The paper develops null-space-based prestructuring so these entries are removed from the sparse pattern before factorization.
## Core Contribution
- **Methodology:** Detects dense rows/columns, projects them out via null-space/prestructuring transformations, factorizes the remaining sparse core, then recovers the solution (approximate).
- **Assumptions:** Dense rows/columns are few relative to matrix size; the residual core stays sparse (approximate).
- **Benchmarks/datasets:** Sparse industrial matrices with dense rows/columns (qualitative; no numbers asserted).
- **Metrics:** nnz and factorization time with vs. without prestructuring (qualitative).
- **Key results:** Large reductions in factorization cost for matrices contaminated by dense rows/columns (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Dense-row/column detection followed by projection so sparse LU/Cholesky sees only the sparse core.
**Techniques:** Null-space projection, dense-column splitting, row/column permutation to expose structure.
**Implementation details:** Highly relevant to refinery/planning models (R11): detect dense columns at read time in src/io/mps.cpp or in presolve (src/presolve/presolve.cpp) and choose a strategy (split, hoist, or reorder) instead of letting one dense column blow up every factorization.
**Equations/rules:** If A = [A₁ A₂] with A₁ dense columns, solve on the reduced system and back-substitute for A₁'s variables (Sparsity).
**Limitations/failure cases:** Projection adds algebraic overhead and can worsen conditioning; only pays off when dense entities are genuinely few (approximate).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R12's industrial models routinely contain dense linking rows/columns; without a stated policy, one dense column silently degrades every basis refactorization and IPM normal equation (R6, R9, R20).
## Evidence → Engineering Decision
- *Finding:* A few dense rows/columns dominate sparse factorization cost → *PS requirement:* R12 → *Component:* src/presolve/presolve.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Oren-1980-Automatic-Scaling-Matrices]] [[Gilbert-1988-Sparse-Partial-Pivoting]] [[Ponte-2026-Good-Fast-Row-Sparse]]
## Uses
- [[Sparsity]] [[Presolve]]
