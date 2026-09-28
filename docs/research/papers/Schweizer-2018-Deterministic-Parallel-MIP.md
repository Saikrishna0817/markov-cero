---
type: paper
title: "Deterministic Parallel MIP"
authors: "Schweizer et al."
year: 2018
venue: "(unverified)"
doi: "(unverified)"
domain: [parallel]
priority: ○
status: standard
tags: [paper, parallel]
---
# Deterministic Parallel MIP
> Makes parallel branch-and-bound reproducible: same instance, same settings, same answer and same search path regardless of thread count.
## Metadata
| Field | Value |
|---|---|
| Authors | Schweizer et al. |
| Year | 2018 |
| Venue | (unverified) |
| DOI/URL | (unverified) |

## Problem Addressed
Parallel MIP is non-deterministic by default: scheduling order changes which node is expanded first, so results, incumbents and runtimes vary between identical runs. That variability makes benchmarking, debugging and regression testing unreliable — exactly what a solver claiming measured performance must avoid.
## Core Contribution
- **Methodology:** Deterministic parallel execution through controlled node-order determinism (fixed tie-breaking, ordered bound updates) plus optional replay of the serial decision sequence; quantify the cost of determinism versus free parallelism.
- **Assumptions:** LP solves themselves are deterministic; synchronization points are inserted only where decisions must be ordered.
- **Benchmarks/datasets:** MIPLIB-style instances (exact tables not re-verified).
- **Metrics:** Run-to-run identity of tree, objective and node count; slowdown versus non-deterministic mode.
- **Key results:** Deterministic parallelism is achievable at modest performance cost, making parallel runs auditable (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Deterministic parallel B&B with ordered node selection and barrier-style bound propagation.
**Techniques:** Stable tie-breaking on (bound, depth, index); deterministic reduction of incumbent updates; replay logging for debugging.
**Implementation details:** phase4.json already reports identical_optima across thread counts but nothing stronger; a deterministic mode in `src/milp/parallel_tree_search.cpp` would let us attribute regressions to code changes rather than to scheduling noise.
**Limitations/failure cases:** Determinism costs synchronization; on performance-critical runs SCIP-style solvers keep a free mode as default.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R15/R16 demand credible benchmark numbers; without run-to-run determinism our speedup figures (0.56× today) cannot be trusted as code-quality signals.
## Evidence → Engineering Decision
- *Finding:* phase4.json records one 4-thread timing with no repeated-run dispersion → *PS requirement:* R15 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* run-to-run variance of runtime and node count
## Related Papers
- [[Berthold-2019-Parallel-SCIP-UG]]
- [[Lodi-2013-Performance-Variability-Mixed]]
## Uses
- [[Branch and Bound]]
