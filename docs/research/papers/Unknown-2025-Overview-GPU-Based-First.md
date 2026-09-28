---
type: paper
title: "An Overview of GPU-based First-Order Methods for LP and Extensions"
authors: "(authors not stated in source list)"
year: 2025
venue: "arXiv"
doi: "(unverified)"
domain: [gpu, parallel]
priority: ✦
status: standard
tags: [paper, gpu]
---
# An Overview of GPU-based First-Order Methods for LP and Extensions
> Survey mapping the family of GPU first-order LP solvers (PDHG variants, restarted schemes, augmented-Lagrangian and ADMM-style methods) and how they relate.
## Metadata
| Field | Value |
|---|---|
| Authors | Not stated in the source list (verify: arXiv 2404.05878) |
| Year | 2025 (as listed; arXiv id 2404.05878 implies initial submission 2024-04) |
| Venue | arXiv |
| DOI/URL | https://ar5iv.labs.arxiv.org/html/2404.05878 |

## Problem Addressed
Many GPU LP solvers look similar but differ in algorithm (primal-dual, restarted, augmented Lagrangian), in what they accelerate (iteration vs. factorization) and in the accuracy they reach. The survey organizes this space so implementers can choose rather than copy a single codebase.
## Core Contribution
- **Methodology:** Taxonomy of GPU first-order LP methods: algorithmic variants, restart/step-size policies, preconditioning/scaling, stopping rules, and how each relates to CPU PDLP and to simplex/IPM hybrids.
- **Assumptions:** Reader knows LP duality and first-order complexity; focus is large sparse LPs.
- **Benchmarks/datasets:** The instance suites used by the surveyed solvers (Netlib plus larger sets; not re-verified).
- **Metrics:** Iterations/second, time to tolerance, scaling with problem size, achievable accuracy.
- **Key results:** Converging view: restarted primal-dual methods plus scaling are the default; gains come from kernels and from hybrid handoff to simplex for high accuracy (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** PDHG/restarted variants, [[ADMM]]-style and augmented-Lagrangian LP solvers.
**Techniques:** Restart policies, step-size normalization, scaling before iteration, residual-based termination.
**Implementation details:** Helps us place `src/lp/first_order/pdlp.cpp` relative to alternatives before investing in kernel work; also lists accuracy limitations that justify keeping our simplex path.
**Limitations/failure cases:** Survey-level: no single configuration dominates; accuracy on degenerate LPs remains first-order's weak spot.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R8 requires choosing *where* GPU acceleration pays; the survey frames the option space for our second engine and for the crossover policy in `docs/gpu.md`.
## Evidence → Engineering Decision
- *Finding:* Our GPU engine wins only past a measured crossover (docs/gpu.md §5) → *PS requirement:* R8 → *Component:* src/lp/first_order/pdlp.cpp → *Metric:* [[KKT Residual]]-matched runtime by instance size
## Related Papers
- [[Lu-2025-cuPDLP-GPU-Implementation]]
- [[Unknown-n.d.-Accelerating-Optimization-Solvers]]
## Uses
- [[Primal-Dual Hybrid Gradient]]
