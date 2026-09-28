---
type: paper
title: "A Polynomial Algorithm in Linear Programming"
authors: "Khachiyan"
year: 1979
venue: "(not listed in source)"
doi: "(unverified)"
domain: [ipm]
priority: ★
status: deep
tags: [paper, ipm]
---

# A Polynomial Algorithm in Linear Programming

> First proof that LP is solvable in polynomial time via the ellipsoid method.

## Metadata
| Field | Value |
|---|---|
| Authors | Khachiyan |
| Year | 1979 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |

## Problem Addressed
Simplex had no worst-case polynomial bound; whether LP admits a polynomial algorithm was open. Khachiyan applied the ellipsoid method (Nemirovski/Yudin) to LP through a separation oracle. The result reframed the complexity of linear programming and triggered the search for practical polynomial methods.

## Core Contribution
- **Methodology:** Maintain an enclosing ellipsoid, cut it with a violated constraint of `A x <= b`, shrink volume polynomially; optimize the objective by bisection on level sets using the feasibility oracle.
- **Assumptions:** Rational data of bit-length L; bounded level sets for bisection; exact separation oracle; no sparsity assumption (dense geometry).
- **Benchmarks/datasets:** Small constructed LPs (historical demonstrations only).
- **Metrics:** Bit-complexity in n (dimension) and L (input length); number of oracle calls.
- **Key results:** Polynomial iteration/bit bound (the originally published exponent was later revised; treat exact powers as (approximate)); practical run times orders of magnitude worse than simplex.

## Engineering-Relevant Knowledge
**Algorithms:** Ellipsoid method; feasibility-by-bisection.
**Techniques:** Separation oracles; volume-reduction arguments; directed rounding for oracle decisions.
**Implementation details:** Each cut updates dense center/covariance — O(n^2) work per iteration, incompatible with the sparse CSC representation in `src/model/model.cpp`.
**Limitations/failure cases:** Numerically fragile; rounding errors invalidate the volume argument unless done in extended precision; never used as a production LP engine.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Foundational for the complexity side of [[Interior-Point Method]] history, but no code path in markov-cero needs ellipsoid logic. Use it when arguing *why* the PS names interior-point methods rather than ellipsoid: complexity theory (R4) versus practical throughput.

## Evidence → Engineering Decision
- *Finding:* Ellipsoid is polynomial but empirically far slower than simplex/first-order methods → *PS requirement:* R4 (interior-point methods as required engine) → *Component:* src/lp/first_order/pdlp.cpp (engine-choice rationale; no ellipsoid module planned) → *Metric:* geometric mean runtime on Netlib LP Collection.
- *Finding:* Dense ellipsoid updates ignore sparsity → *PS requirement:* R6 (sparse matrix techniques) → *Component:* src/model/model.cpp (CSC immutability) → *Metric:* nonzeros touched per solve.

## Related Papers
- [[Karmarkar-1984-New-Polynomial-Time-Algorithm]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]

## Uses
- [[Interior-Point Method]]
- [[Netlib LP Collection]]
