---
type: paper
title: "A Simple, Quadratically Convergent Interior Point Algorithm for LP and Convex QP"
authors: "Tits & Zhou"
year: 1994
venue: "Large Scale Optimization: State of the Art (Kluwer), pp. 411-427"
doi: "10.1007/978-1-4613-3632-7_20"
domain: [ipm, qp]
priority: ✦
status: standard
tags: [paper, qp]
---
# A Simple, Quadratically Convergent Interior Point Algorithm for LP and Convex QP

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> One algorithm for both LP and convex QP with quadratic (not just linear) local convergence near the solution.
## Metadata
| Field | Value |
|---|---|
| Authors | Tits & Zhou |
| Year | 1994 |
| Venue | Large Scale Optimization: State of the Art (Kluwer), pp. 411-427 |
| DOI/URL | 10.1007/978-1-4613-3632-7_20 |
## Problem Addressed
Primal-dual path-following methods converge linearly near the optimum, so high accuracy costs extra iterations. Tits & Zhou sought a simple scheme with superlinear/quadratic final convergence that works uniformly for LP and convex QP.
## Core Contribution
- **Methodology:** Interior-point variant with an adaptive centering/step rule yielding quadratic local convergence; the QP case adds the quadratic term `P x` to the dual feasibility equations, otherwise identical machinery.
- **Assumptions:** Convex QP (PSD P; strictly convex for uniqueness); strictly feasible/central start; accurate linear solves.
- **Benchmarks/datasets:** Large-scale LP and convex QP test problems (paper's set).
- **Metrics:** Iterations to high accuracy; local convergence order.
- **Key results:** Quadratic local convergence reported; simple iteration form (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Quadratically convergent primal-dual IPM for LP/convex QP.
**Techniques:** Adaptive step/centering parameters; unified LP-QP formulation.
**Implementation details:** markov-cero's QP engine is ADMM (`src/qp/admm_solver.cpp`), which converges only linearly and stops at a loose [[KKT Residual]] tolerance by design — an IPM-QP is the high-accuracy alternative.
**Limitations/failure cases:** Requires accurate Newton solves to realize local order; quadratic convergence is local, not global.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R2 (QP in scope) + R3 (MIQP path) + R9. Useful if ADMM's accuracy proves insufficient for tight optimality certificates on small/medium QPs.
## Evidence → Engineering Decision
- *Finding:* ADMM gives moderate accuracy cheaply; IPM-QP gives high accuracy in few final iterations → *PS requirement:* R2, R17 → *Component:* src/qp/admm_solver.cpp (tolerance contract) + src/qp/verifier.cpp → *Metric:* [[KKT Residual]] and [[Duality Gap]] reported by the QP verifier.
## Related Papers
- [[Wright-1997-Primal-Dual-IPM]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
## Uses
- [[Interior-Point Method]]
