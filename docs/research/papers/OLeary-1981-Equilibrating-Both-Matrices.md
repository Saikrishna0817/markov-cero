---
type: paper
title: "Equilibrating Both Matrices in Linearly Dependent Problems"
authors: "O'Leary"
year: 1981
venue: "(unverified)"
doi: "(unverified)"
domain: [scaling, numerics]
priority: ○
status: standard
tags: [paper, scaling, numerics]
---

# Equilibrating Both Matrices in Linearly Dependent Problems

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Equilibration designed for rank-deficient/dependent systems where scaling a single matrix is unstable or meaningless.
## Metadata
| Field | Value |
|---|---|
| Authors | O'Leary |
| Year | 1981 |
| Venue | (unverified) |
| DOI/URL | (unverified) |
## Problem Addressed
Redundant and linearly dependent rows are common in industrial models; ordinary equilibration applied to a single matrix can fail to converge or distort the dependency structure, leaving the factorization still ill-conditioned. The paper treats equilibration for problems where the coefficient matrix and its dependent structure must be scaled together.
## Core Contribution
- **Methodology:** Simultaneous equilibration of the (possibly rank-deficient) coefficient matrix together with its dependent row/column structure, with perturbation analysis showing when scaling remains well defined.
- **Assumptions:** Matrices may be rank-deficient or nearly so; scaling should preserve dependency relations exactly (approximate).
- **Benchmarks/datasets:** Constructed linearly dependent systems (qualitative; no figures asserted).
- **Metrics:** Residual norms and conditioning after scaling (qualitative).
- **Key results:** A robust equilibration procedure that behaves sensibly when plain row/column scaling breaks down (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Joint equilibration of A and the augmented/dependent structures appearing in constraint systems.
**Techniques:** Dependency-aware scaling, tolerance-aware detection of dependent rows before scaling.
**Implementation details:** Interacts with presolve redundancy detection (src/presolve/presolve.cpp) and the scaling pass: decide order — detect dependency, scale without breaking it, then refactor.
**Equations/rules:** If A x = b has dependent rows, scale row pairs so the row space (and hence the feasible set) is preserved; a row scale factor must be applied to b in lockstep.
**Limitations/failure cases:** Niche case — on well-posed problems plain equilibration suffices; extra analysis cost buys little (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R13 targets highly degenerate/redundant industrial models (R11) where dependent rows appear; knowing how to scale without destroying dependency prevents presolve/scaling from fighting each other.
## Evidence → Engineering Decision
- *Finding:* Dependent rows need dependency-preserving scaling, not blind equilibration → *PS requirement:* R13 → *Component:* src/presolve/presolve.cpp → *Metric:* Numerical Error
## Related Papers
- [[Oren-1980-Automatic-Scaling-Matrices]] [[Maros-2003-Computational-Optimization-Techniques]] [[Howell-2018-Prestructuring-Sparse-Matrices]]
## Uses
- [[Scaling]] [[Ill-Conditioning]]
