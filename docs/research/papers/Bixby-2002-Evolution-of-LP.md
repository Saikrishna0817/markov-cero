---
type: paper
title: "Computational Linear Programming: The Evolution of LP Solvers"
authors: "Bixby"
year: 2002
venue: "Operations Research"
doi: "(unverified)"
domain: [survey, lp]
priority: ★
status: deep
tags: [paper, survey, lp]
---

# Computational Linear Programming: The Evolution of LP Solvers

> Decomposes four decades of LP speedups into attributable contributions of presolve, sparse factorization, dual simplex and steepest-edge pricing.

## Metadata
| Field | Value |
|---|---|
| Authors | Bixby |
| Year | 2002 |
| Venue | Operations Research |
| DOI/URL | (unverified) |

## Problem Addressed
LP solving improved by orders of magnitude over roughly forty years, but practitioners could not tell which innovations mattered. The survey isolates the contributions of presolve, sparse LU with threshold pivoting, the dual simplex, steepest-edge pricing and crossover, so a solver builder knows where engineering effort actually pays.

## Core Contribution
- **Methodology:** Historical survey plus timing studies on successive solver generations, attributing speedup multiplicatively to individual features (approximate).
- **Assumptions:** Benchmark LPs representative of industrial practice; hardware differences normalized (approximate).
- **Benchmarks/datasets:** Netlib and industrial LP collections (approximate; no instance list asserted).
- **Metrics:** Wall-clock runtime, factorizations per solve, speedup ratios.
- **Key results:** Cumulative gains of several orders of magnitude since the 1960s, with each major technique contributing a large multiplicative factor (approximate; no specific numbers asserted).

## Engineering-Relevant Knowledge
**Algorithms:** Revised primal simplex, dual simplex, crossover to a basic solution.

**Techniques:** LP presolve, Markowitz + threshold partial pivoting, sparse LU refactorization policy, steepest-edge weight updates.

**Implementation details:** Speedups multiply rather than add — implement presolve, sparsity-aware factorization and dual simplex as separable modules so each can be ablated and measured (this is exactly the R16 comparison methodology).

**Equations/rules:** Cost model: total time ≈ iterations × (price + ratio test) + refactorization cost; each historical advance attacks a different factor.

**Limitations/failure cases:** Attribution is not perfectly causal, and the 1990s–2000s model mix differs from modern MIPLIB/QPLIB instances (approximate).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is a prioritization blueprint for a from-scratch solver: presolve, sparse LU, dual simplex and steepest edge must all exist before micro-optimization, and every claimed gain needs an ablation measurement (R4, R6, R9, R16).

## Evidence → Engineering Decision
- *Finding:* Each of presolve, sparse factorization, dual simplex and steepest-edge pricing yields a large multiplicative speedup → *PS requirement:* R4 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Gains must be attributed by ablation, not anecdote → *PS requirement:* R16 → *Component:* benchmarks/ → *Metric:* Geometric Mean Runtime

## Related Papers
- [[Wright-2004-Interior-Point-Revolution]]
- [[Achterberg-2007-Constraint-Integer-Programming]]
- [[Koberstein-2005-Dual-Simplex-Method]]
- [[Bartels-1969-Simplex-LU-Decomposition]]

## Uses
- [[Sparse LU]]
- [[Steepest Edge]]
- [[Presolve]]
