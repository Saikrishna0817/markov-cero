---
type: paper
title: "cuPDLP.jl: A GPU Implementation of Restarted PDHG for LP"
authors: "Lu & Yang"
year: 2025
venue: "Operations Research"
doi: "10.1287/opre.2024.1069"
domain: [gpu, parallel]
priority: ✦
status: standard
tags: [paper, gpu]
---
# cuPDLP.jl: A GPU Implementation of Restarted PDHG for LP

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Demonstrates that a restarted primal-dual hybrid gradient method on a GPU can solve large LPs at speeds no CPU simplex reaches — the direct ancestor of our PDHG engine.
## Metadata
| Field | Value |
|---|---|
| Authors | Lu & Yang |
| Year | 2025 |
| Venue | Operations Research |
| DOI/URL | 10.1287/opre.2024.1069 |

## Problem Addressed
First-order methods are iteration-inefficient on LP but map perfectly to GPUs; the question is whether restarts, step-size rules and careful data movement turn that mapping into wall-clock wins over mature CPU solvers on large, sparse LPs.
## Core Contribution
- **Methodology:** Restarted PDHG for LP (adaptive restart, normalization of steps), implemented in Julia on CUDA with CSR kernels, presolving/scaling, and a residual-based stopping rule; hybridization with simplex-style refinement considered for accuracy.
- **Assumptions:** Large sparse LPs where memory bandwidth dominates; tolerance-based (not exact) solutions acceptable.
- **Benchmarks/datasets:** Netlib and large-scale LP collections (exact tables not re-verified here).
- **Metrics:** Time to target primal-dual residual, scaling with rows/cols, comparison vs. CPU solvers.
- **Key results:** Order-of-magnitude wins on large instances, losses on small ones — the empirical shape our crossover table reproduces (specific figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** [[Primal-Dual Hybrid Gradient]] with restarts for LP.
**Techniques:** Restart scheduling, step-size adaptation, on-device iteration to minimize transfers, scaling before iteration (Ruiz-style) and residual monitoring.
**Implementation details:** Our `src/lp/first_order/pdlp.cpp` + `gpu/kernels/pdhg_step.cu` implement the same algorithm family; `docs/gpu.md` shows kernel ≈ 22.8 ms of 24.2 ms total at SCALE_10000 — transfer is already amortized, iteration efficiency is the remaining gap.
**Limitations/failure cases:** Loses to simplex on tiny/degenerate LPs (AFIRO, SC50A in our crossover table); accuracy limited by first-order convergence rate near degenerate optima.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the reference implementation pattern for our GPU engine and the primary evidence that R8's "measurable benefit" is real only above a size threshold we have measured ourselves.
## Evidence → Engineering Decision
- *Finding:* CPU simplex beats GPU PDLP on AFIRO/SC50A but GPU wins from SC50B upward (evidence/benchmarks/crossover_study.csv) → *PS requirement:* R8 → *Component:* gpu/kernels/pdhg_step.cu → *Metric:* total_ms vs. simplex ms ([[KKT Residual]]-matched)
## Related Papers
- [[Lin-2025-PDCS-Primal-Dual]]
- [[Bell-2008-Efficient-Sparse-Matrix]]
## Uses
- [[Adaptive Restart]]
