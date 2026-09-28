---
type: paper
title: "PDCS: A Primal-Dual Large-Scale Conic Programming Solver with GPU Enhancements"
authors: "Lin, Xiong, Ge & Ye"
year: 2025
venue: "(unverified)"
doi: "(unverified)"
domain: [gpu, parallel]
priority: ✦
status: standard
tags: [paper, gpu]
---
# PDCS: A Primal-Dual Large-Scale Conic Programming Solver with GPU Enhancements
> A GPU primal-dual first-order solver for large LP/QP/conic problems reported to beat commercial solvers at scale — evidence for a two-engine architecture.
## Metadata
| Field | Value |
|---|---|
| Authors | Lin, Xiong, Ge & Ye |
| Year | 2025 |
| Venue | (unverified) |
| DOI/URL | https://arxiv.org/abs/2505.00311 |

## Problem Addressed
Commercial simplex/IPM solvers degrade on very large, sparse conic models while first-order methods scale with matrix-vector products. The paper shows how to engineer a GPU primal-dual method (data structures, step-size adaptation, kernels) into a competitive general-purpose conic solver.
## Core Contribution
- **Methodology:** First-order primal-dual algorithm extended to conic constraints, with problem prescaling, adaptive steps, and GPU kernels for the per-iteration linear algebra; comparison against commercial solvers on large instances.
- **Assumptions:** Large sparse problem size justifies GPU transfer costs; moderate solution accuracy (first-order residuals) is acceptable.
- **Benchmarks/datasets:** Large LP/QP/conic instances (specific suites not re-verified here).
- **Metrics:** Wall-clock to target residual, scaling with problem size, comparison vs. commercial solver runtimes.
- **Key results:** Reported wins over commercial solvers on the largest instances while losing on small ones (per source list; numbers not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Primal-dual first-order method for conic programs (LP/QP as special cases).
**Techniques:** Prescaling, adaptive step sizes, GPU-resident iteration with sparse CSR data — same family as our PDHG engine.
**Implementation details:** Mirrors our `src/lp/first_order/pdlp.cpp` + `gpu/kernels/pdhg_step.cu` design and supports the multi-engine claim: first-order GPU for large, simplex for small.
**Limitations/failure cases:** First-order methods reach only moderate accuracy; crossover to simplex/crossover phase needed for exact bases and for degenerate instances.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** It validates the architectural bet behind `docs/gpu.md` — a GPU first-order engine for large problems — and provides comparison methodology versus commercial solvers (R16) we currently lack.
## Evidence → Engineering Decision
- *Finding:* Our GPU engine wins only past a crossover size (AFIRO 0.9×, SC50A 0.7× vs simplex; docs/gpu.md §5) → *PS requirement:* R8 → *Component:* src/lp/first_order/pdlp.cpp → *Metric:* [[Geometric Mean Runtime]] by size band
## Related Papers
- [[Lu-2025-cuPDLP-GPU-Implementation]]
- [[Unknown-2025-Overview-GPU-Based-First]]
## Uses
- [[Multi-Engine Solver Architecture]]
