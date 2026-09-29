---
type: paper
title: "Sparse Partial Pivoting in Time Proportional to Arithmetic Operations"
authors: "Gilbert & Peierls"
year: 1988
venue: "SIAM J. Sci. Stat. Comput."
doi: "(unverified)"
domain: [sparse]
priority: ★
status: deep
tags: [paper, sparse]
---

# Sparse Partial Pivoting in Time Proportional to Arithmetic Operations

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Sparse triangular solve (FTRAN/BTRAN) that costs time proportional to the entries actually touched.

## Metadata
| Field | Value |
|---|---|
| Authors | Gilbert & Peierls |
| Year | 1988 |
| Venue | SIAM J. Sci. Stat. Comput. |
| DOI/URL | (unverified) |

## Problem Addressed
A sparse triangular solve implemented naively scans rows or columns that the output never touches, so runtime can dwarf the arithmetic actually required. For LP this matters enormously: pricing and the ratio test perform a triangular solve for every candidate column, so an "O(n²)" solve hidden inside a sparse code silently caps iteration speed.

## Core Contribution
- **Methodology:** Column-oriented traversal of L (or U) driven by the nonzero pattern of the right-hand side, computing the exact reach set and applying partial pivoting checks only where elimination actually proceeds.
- **Assumptions:** Triangular factors stored in compressed sparse form; right-hand side is itself sparse in LP pricing (hyper-sparsity) (approximate).
- **Benchmarks/datasets:** Sparse matrices of the 1980s plus LP pricing contexts (qualitative; no figures asserted).
- **Metrics:** Time/flops ratio — runtime bounded by arithmetic performed, not matrix dimension.
- **Key results:** Proves a solve runs in time proportional to the product of nnz in the factor column and in the output — i.e., cost = computation (approximate statement of the theorem).

## Engineering-Relevant Knowledge
**Algorithms:** Sparse forward substitution (FTRAN) and backward substitution (BTRAN) with partial pivoting.

**Techniques:** Reach computation via graph traversal/DFS, index-stack marking to avoid duplicate work, symbolic prediction of the output pattern.

**Implementation details:** Every basis solve in src/linalg/sparse_basis.cpp should be written in this style: traverse only the columns that can affect the result; use the same kernel for pricing columns, ratio-test columns and dual-price recovery (BTRAN adjoint).

**Equations/rules:** For Lx = b with |L| the pattern, x is nonzero only on the reach of b in the directed graph of L; time ∝ Σ_j (nnz(L_col_j)·[j ∈ reach]).

**Limitations/failure cases:** Partial pivoting can enlarge nonzeros and break the predicted pattern; dense rows/columns destroy proportionality (see Howell's prestructuring work); correctness depends on exact integer index bookkeeping.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R6 demands sparse techniques and R12 million-scale models; this algorithm is the floor-level determinant of simplex throughput — if our solves are not proportional to touched entries, no pricing or steepest-edge choice can recover the lost time (R20).

## Evidence → Engineering Decision
- *Finding:* Solve time should scale with touched entries, not matrix dimension → *PS requirement:* R6 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Pivoting must not silently break the sparsity pattern assumed by the solver → *PS requirement:* R13 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error

## Related Papers
- [[Bartels-1969-Simplex-LU-Decomposition]]
- [[Curtis-1972-Simplex-LU-Decomposition]]
- [[Amestoy-1996-Approximate-Minimum-Degree]]
- [[Hall-2005-Hyper-sparsity-Revised-Simplex]]

## Uses
- [[Sparse LU]]
- [[Sparsity]]
