---
type: paper
title: "The Interior-Point Revolution in Optimization"
authors: "Wright"
year: 2004
venue: "SIAM Review"
doi: "10.1090/s0273-0979-04-01040-7"
domain: [survey, lp]
priority: ★
status: deep
tags: [paper, survey, lp]
---

# The Interior-Point Revolution in Optimization

> Explains how barrier and path-following methods displaced — but did not replace — simplex, and what each engine is still good for.

## Metadata
| Field | Value |
|---|---|
| Authors | Wright |
| Year | 2004 |
| Venue | SIAM Review |
| DOI/URL | 10.1090/s0273-0979-04-01040-7 |

## Problem Addressed
Karmarkar's 1984 algorithm overturned the belief that LP required exponential worst-case simplex, yet the field lacked one account linking ellipsoid complexity, self-concordance and path-following to observed solver behavior. Wright's SIAM Review article supplies that narrative and explains why simplex survived the "revolution".

## Core Contribution
- **Methodology:** Survey synthesizing complexity theory (ellipsoid method, Karmarkar's projective method, self-concordant barriers) with computational practice.
- **Assumptions:** Convex LP setting; polynomial-time claims are worst-case and do not predict practical runtimes (approximate).
- **Benchmarks/datasets:** Qualitative references to contemporary solver experience (approximate; no instances asserted).
- **Metrics:** Iteration complexity vs. observed iteration counts.
- **Key results:** Path-following primal-dual methods converge in O(√n · log(1/ε)) iterations (approximate); barrier methods take tens of expensive iterations where simplex takes thousands of cheap ones, yet simplex still wins on many sparse LPs (approximate/qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Primal-dual path-following interior-point method; simplex as the competing engine.

**Techniques:** Symmetric indefinite (LDLᵀ) systems vs. normal equations; crossover to a vertex.

**Implementation details:** An IPM needs a different linear-algebra stack (Cholesky/LDLᵀ with repeated refactorization) than simplex (LU with triangular updates); a from-scratch solver should share only the sparse matrix core between engines, not the factorization path.

**Equations/rules:** Barrier subproblems minimize -Σ log xᵢ subject to Ax=b; centrality/duality gap μ tracks O(√n · ε) (approximate).

**Limitations/failure cases:** Barrier returns an interior, non-basic point — unusable for MIP branching until crossover; sensitive to dense columns (approximate).

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R4 mandates both revised simplex and interior-point; Wright fixes the division of labor (barrier for large sparse LP, simplex for bases and MIP nodes) that our two engines — src/lp/first_order/pdlp.cpp and src/lp/dual/dual_simplex.cpp — must respect.

## Evidence → Engineering Decision
- *Finding:* Barrier methods converge in few but expensive iterations; simplex in many cheap ones → *PS requirement:* R4 → *Component:* src/lp/first_order/pdlp.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Interior points must be driven to a vertex for MIP use → *PS requirement:* R5 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* KKT Residual

## Related Papers
- [[Bixby-2002-Evolution-of-LP]]
- [[Achterberg-2007-Constraint-Integer-Programming]]
- [[Dantzig-1963-Linear-Programming-Extensions]]

## Uses
- [[Interior-Point Method]]
- [[Crossover]]
