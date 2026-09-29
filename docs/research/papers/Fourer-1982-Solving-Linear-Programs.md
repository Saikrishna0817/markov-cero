---
type: paper
title: "Solving Linear Programs by Dual Simplex"
authors: "Fourer"
year: 1982
venue: "(unverified)"
doi: "(unverified)"
domain: [lp]
priority: ○
status: standard
tags: [paper, lp]
---

# Solving Linear Programs by Dual Simplex

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Standard exposition of the dual simplex method as an LP solution technique.
## Metadata
| Field | Value |
|---|---|
| Authors | Fourer (list also cites Dantzig/Cottle/Lemke eds.) |
| Year | 1982 |
| Venue | (unverified) |
| DOI/URL | (unverified) |
## Problem Addressed
The primal simplex requires a feasible starting basis, which is exactly what re-optimization after bound or RHS changes does not have; the dual simplex solves from an infeasible basis while maintaining dual feasibility, making it the natural engine for repeated solves.
## Core Contribution
- **Methodology:** Exposition of the dual simplex algorithm — primal-feasibility restoration via dual ratio test, with the duality-gap bookkeeping that makes it a counterpart of the primal method (approximate).
- **Assumptions:** Standard-form LP; existing dual-feasible basis; bounded objective for the dual direction (approximate).
- **Benchmarks/datasets:** None asserted (expository chapter).
- **Metrics:** Not applicable (qualitative).
- **Key results:** Establishes the dual simplex as a standard, implementable LP method rather than a theoretical curiosity (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Dual revised simplex; warm-started re-optimization.
**Techniques:** Dual ratio test, basis feasibility restoration, dual-variable recovery.
**Implementation details:** Documents the iteration skeleton implemented in src/lp/dual/dual_simplex.cpp: while the basic solution violates primal bounds, pick an leaving row by the dual ratio test and re-enter a column that restores that bound.
**Equations/rules:** Leaving row r chosen by max violation; entering column minimizes Δz = c̄ⱼ·(violation)/(ȳᵢ) over candidates with ȳᵢ of correct sign (Dual Simplex).
**Limitations/failure cases:** Pedagogical treatment; says nothing about tolerances, degeneracy handling or sparse implementation — those come from Harris, Koberstein and the Module 2 papers.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Gives the clean algorithmic statement we can derive our implementation from (R10: from mathematical foundation), while the engineering hardening must come from the deeper references.
## Evidence → Engineering Decision
- *Finding:* Warm-started dual re-optimization is the natural node-LP engine → *PS requirement:* R5 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Koberstein-2005-Dual-Simplex-Method]] [[Maros-2003-Generalized-Dual-Phase]] [[Goldfarb-1992-Steepest-Edge-Simplex]]
## Uses
- [[Dual Simplex]] [[Warm Start]]
