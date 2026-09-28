---
type: paper
title: "Design and Implementation of a Reduced-Space SQP Solver with Column Reordering for Large-Scale Process Optimization"
authors: "Zhao, Liu, Jiang et al."
year: 2025
venue: "Algorithms"
doi: "(unverified)"
domain: [sparse]
priority: ✦
status: standard
tags: [paper, sparse]
---

# Design and Implementation of a Reduced-Space SQP Solver with Column Reordering for Large-Scale Process Optimization
> Shows column reordering plus sparse kernels inside a practical large-scale process-optimization solver.
## Metadata
| Field | Value |
|---|---|
| Authors | Zhao, Liu, Jiang et al. |
| Year | 2025 |
| Venue | Algorithms |
| DOI/URL | https://www.mdpi.com/1999-4893/18/11/699 |
## Problem Addressed
Process-optimization models (refinery/planning-adjacent, R11) are huge and sparse; a reduced-space SQP solver must keep its KKT/linear-algebra steps sparse and choose column ordering so factorizations stay cheap at industrial scale.
## Core Contribution
- **Methodology:** Reduced-space SQP design with explicit column-reordering integration into the sparse factorization path, plus implementation-level engineering detail (approximate).
- **Assumptions:** Smooth nonlinear constraints with exploitable sparsity; reduced-space approach chosen over full-space KKT (approximate).
- **Benchmarks/datasets:** Large-scale process-optimization instances (qualitative; no numbers asserted).
- **Metrics:** Solve time, factorization cost, scaling behavior with model size (qualitative).
- **Key results:** Column reordering integrated with sparse computation is a decisive runtime factor in a working large-scale solver (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Reduced-space SQP with sparse linear algebra; closest analog to how a QP subproblem would be solved inside our MILP/QP path.
**Techniques:** Column reordering, sparse kernel reuse, structured KKT formation.
**Implementation details:** Reinforces that ordering must be wired into the solver's refactorization loop, not bolted on — a design point for src/qp/ and src/linalg/ when QP/MIQP (R2) grows beyond its current scope.
**Equations/rules:** Reorder columns so the factorization of the (reduced) KKT/Hessian structure minimizes fill (Fill-Reducing Ordering).
**Limitations/failure cases:** SQP/NLP-specific; process models are nonlinear — our LP/MILP core only inherits the linear-algebra lessons (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Directly on-scope for R11 (process/refinery optimization) and R6/R7 (sparse, scalable kernels); the reordering-in-the-solver-loop pattern applies to our QP path, though SQP itself is out of scope (R3 lists NLP only as later extension).
## Evidence → Engineering Decision
- *Finding:* Reordering integrated into the solver loop, not as a one-off preprocessing step → *PS requirement:* R6 → *Component:* src/qp/ → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Davis-2004-Column-Approximate-Minimum]] [[Karypis-1998-Fast-High-Quality]] [[Amestoy-1996-Approximate-Minimum-Degree]]
## Uses
- [[Fill-Reducing Ordering]] [[Sparsity]]
