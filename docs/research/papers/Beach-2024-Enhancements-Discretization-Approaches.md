---
type: paper
title: "Enhancements of Discretization Approaches for Non-Convex MIQCQP (Parts I & II)"
authors: "Beach, Burlacu, Bärmann, Hager & Hildebrand"
year: 2024
venue: "Computational Optimization and Applications 87(3)"
doi: "10.1007/s10589-023-00543-7 (Part I); 10.1007/s10589-024-00554-y (Part II)"
domain: [miqp]
priority: ✦
status: standard
tags: [paper, miqp]
---
# Enhancements of Discretization Approaches for Non-Convex MIQCQP (Parts I & II)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Two-part study of discretization-based global methods for non-convex quadratically constrained mixed-integer programs — the machinery needed if quadratic constraints ever enter our scope.
## Metadata
| Field | Value |
|---|---|
| Authors | Beach, Burlacu, Bärmann, Hager & Hildebrand |
| Year | 2024 |
| Venue | Computational Optimization and Applications 87(3) |
| DOI/URL | 10.1007/s10589-023-00543-7 (Part I); 10.1007/s10589-024-00554-y (Part II) |

## Problem Addressed
Non-convex quadratic constraints make MIQCQP hard: convex relaxations leave gaps that branch-and-bound must close by splitting the continuous domain. The work addresses improving discretization-based approaches — partitioning variable domains so McCormick-type relaxations tighten — for practical global solution.
## Core Contribution
- **Methodology:** Part I: algorithmic enhancements of discretization relaxations (domain partitioning, bound tightening, integration into spatial branch-and-bound). Part II: computational evaluation against established global solvers.
- **Assumptions:** Bounded variables (needed for discretization); functions with evaluable convex/concave envelopes.
- **Benchmarks/datasets:** Non-convex MIQCQP test collections (e.g. MINLPLib-style; exact sets not re-verified).
- **Metrics:** Gap closed vs. time, nodes in spatial tree, comparison with global solvers.
- **Key results:** Discretization enhancements improve the tightness/time trade-off over baseline relaxations (figures not re-verified; Errata notes a wrong JGO link in the source repository document for this entry).
## Engineering-Relevant Knowledge
**Algorithms:** Spatial branch-and-bound with discretized relaxations; bound tightening.
**Techniques:** Adaptive domain splitting, relaxation tightening, pruning by rigorous bounds.
**Implementation details:** Far beyond current scope — but it shows what "extended to NLP/MINLP" (R3) would demand: rigorous bounding and a global-search driver around `src/milp/` rather than only local optimization.
**Limitations/failure cases:** Spatial B&B blows up with dimension; non-convexity is not in R2's initial scope at all.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R3 lists MIQP/NLP/MINLP as *later* extensions; this sets expectations for the effort such an extension implies, without changing anything now.
## Evidence → Engineering Decision
- *Finding:* Current QP path is convex/ADMM-only (`src/qp/admm_solver.cpp`), no nonconvex capability → *PS requirement:* R3 → *Component:* src/qp/model.cpp → *Metric:* nonconvex instance gap handling (roadmap only)
## Related Papers
- [[Beach-2022-Compact-Mixed-Integer]]
- [[Kronqvist-2025-50-Years-Mixed-Integer]]
## Uses
- [[Branch and Bound]]
