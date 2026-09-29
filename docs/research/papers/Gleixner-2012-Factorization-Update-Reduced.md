---
type: paper
title: "Factorization and Update of a Reduced Basis Matrix for the Revised Simplex Method"
authors: "Gleixner"
year: 2012
venue: "ZIB"
doi: "(unverified)"
domain: [lp, sparse]
priority: ✦
status: standard
tags: [paper, lp, sparse]
---

# Factorization and Update of a Reduced Basis Matrix for the Revised Simplex Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Factorizes and updates a reduced basis matrix to improve stability and sparsity in the revised simplex.
## Metadata
| Field | Value |
|---|---|
| Authors | Gleixner |
| Year | 2012 |
| Venue | ZIB |
| DOI/URL | https://opus4.kobv.de/opus4-zib/frontdoor/index/index/docId/1681 |
## Problem Addressed
The revised simplex carries the full basis even when only a few rows/columns are active in an iteration; simultaneously, basis updates degrade stability and fill. The thesis develops factorizing and updating a *reduced* basis matrix so both the cost and the numerical damage of basis maintenance shrink together.
## Core Contribution
- **Methodology:** Reduced-basis factorization and update schemes for the revised simplex, analyzed for stability and sparsity effects (approximate).
- **Assumptions:** Only a subset of rows/columns materially participates per iteration; standard-form LP (approximate).
- **Benchmarks/datasets:** LP test sets of the 2010s (qualitative; no figures asserted).
- **Metrics:** Factorization/update cost, fill, numerical accuracy over the solve (qualitative).
- **Key results:** Reduced-basis handling improves both stability and sparsity relative to naive full-basis updates (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex with reduced-basis factorization and structured updates.
**Techniques:** Active-row/column reduction, sparse factorization reuse, stability-monitored updates.
**Implementation details:** An advanced option for src/linalg/sparse_basis.cpp once the baseline (LU + Forrest-Tomlin + threshold pivoting) is measured; also echoes how MIP node LPs reuse structure across nodes (src/milp/node_lp.cpp).
**Equations/rules:** Factorize B_reduced over the active set; update on basis change; full quantities recovered by back-substitution (Basis).
**Limitations/failure cases:** Bookkeeping complexity grows; benefits depend on how sparse iteration activity really is (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R9 (stability) and R6 (sparsity) are usually in tension; reduced-basis methods attack both at once — relevant if measurements show our basis maintenance (R16) is the bottleneck rather than pricing.
## Evidence → Engineering Decision
- *Finding:* Reduced-basis factorization can cut both fill and update error → *PS requirement:* R9 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error
## Related Papers
- [[Forrest-1972-Updating-Triangular-Factors]] [[Bartels-1969-Simplex-LU-Decomposition]] [[Hall-2005-Hyper-sparsity-Revised-Simplex]]
## Uses
- [[Basis]] [[Sparse LU]] [[Numerical Stability]]
