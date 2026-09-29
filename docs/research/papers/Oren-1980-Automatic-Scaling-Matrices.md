---
type: paper
title: "Automatic Scaling of Matrices for Gaussian Elimination"
authors: "Oren"
year: 1980
venue: "(unverified)"
doi: "(unverified)"
domain: [scaling, numerics]
priority: ○
status: standard
tags: [paper, scaling, numerics]
---

# Automatic Scaling of Matrices for Gaussian Elimination

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Row/column equilibration chosen to keep elimination stable, not merely to normalize magnitudes (listed also as "Model Scaling in LP").
## Metadata
| Field | Value |
|---|---|
| Authors | Oren (also cited in list as: Oren, "Model Scaling in LP", 1980) |
| Year | 1980 |
| Venue | (unverified) |
| DOI/URL | (unverified) |
## Problem Addressed
Unscaled LP and Gaussian-elimination matrices mix entries spanning many orders of magnitude, causing overflow/underflow, oversized growth factors and loss of significant digits in factorization; a principled automatic rule is needed before elimination runs.
## Core Contribution
- **Methodology:** Analysis of how row/column scaling changes element growth and conditioning during elimination, with automatic equilibration rules derived from that analysis.
- **Assumptions:** Sparse matrices as they arise in LP; scaling is applied once (or iteratively) before factorization (approximate).
- **Benchmarks/datasets:** LP test problems of the era (qualitative; no instance list asserted).
- **Metrics:** Element growth factor, condition estimate, accuracy of the computed solution (qualitative).
- **Key results:** Appropriate scaling materially reduces growth during elimination and is the cheapest available robustness improvement (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Pre-factorization row/column scaling of the basis or full constraint matrix.
**Techniques:** Max-norm/RMS equilibration, iterative row-then-column rescaling to convergence, unscale-on-output.
**Implementation details:** Must run before factorization in src/linalg/sparse_basis.cpp and be exactly inverted for primal/dual solution recovery; the iterative form is what src/scale/ruiz_scaling.cpp implements.
**Equations/rules:** Scale row i by 1/max|aᵢⱼ| and column j by 1/max|aᵢⱼ| alternately until magnitudes cluster near 1; multiply objective/RHS/bounds consistently so the optimal point is unchanged.
**Limitations/failure cases:** Scaling cannot repair structural ill-conditioning or rank deficiency; aggressive scaling can magnify small pivots; scaling interacts with tolerances (approximate).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R13 calls out ill-conditioned matrices explicitly and R9 demands numerical stability; scaling is the single cheapest mitigation, already partly present in src/scale/ruiz_scaling.cpp and needing an evidence-backed justification.
## Evidence → Engineering Decision
- *Finding:* Scaling is the cheapest robustness win against ill-conditioning → *PS requirement:* R13 → *Component:* src/scale/ruiz_scaling.cpp → *Metric:* Numerical Error
## Related Papers
- [[OLeary-1981-Equilibrating-Both-Matrices]] [[Curtis-1972-Simplex-LU-Decomposition]] [[Maros-2003-Computational-Optimization-Techniques]]
## Uses
- [[Scaling]] [[Ruiz Scaling]] [[Ill-Conditioning]]
