---
type: paper
title: "PGAS-based Parallel Branch-and-Bound for Ultra-Scale GPU-powered Supercomputers"
authors: "Helbecque"
year: 2025
venue: "(unverified)"
doi: "(unverified)"
domain: [gpu, parallel]
priority: ✦
status: standard
tags: [paper, gpu]
---
# PGAS-based Parallel Branch-and-Bound for Ultra-Scale GPU-powered Supercomputers
> Compares PGAS versus MPI+X programming models for multi-level (GPU + inter-node) branch-and-bound at exascale.
## Metadata
| Field | Value |
|---|---|
| Authors | Helbecque |
| Year | 2025 |
| Venue | (unverified) |
| DOI/URL | https://orbilu.uni.lu/handle/10993/63097 |

## Problem Addressed
On GPU-equipped supercomputers, branch-and-bound must coordinate thousands of nodes *and* thousands of GPU threads per node. The paper asks which programming model (PGAS with a global address space vs. MPI + GPU-accelerator layers) supports that multi-level parallelism with least communication overhead.
## Core Contribution
- **Methodology:** Implement the same parallel B&B in a PGAS model and in MPI+X; compare scalability of node exchange, bound propagation and GPU offload across levels.
- **Assumptions:** Distributed memory with GPU devices per node; large trees (exascale-scale instances).
- **Benchmarks/datasets:** Large-scale MIP/optimization workloads (exact sets not re-verified).
- **Metrics:** Scaling curves, communication volume, time-to-solution across node counts.
- **Key results:** Trade-offs reported between programmability of PGAS and maturity/tuning of MPI+X; neither dominates universally (details not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Multi-level parallel B&B (intra-node GPU parallelism + inter-node task distribution).
**Techniques:** Partitioned global address space to express node-pool sharing without explicit messages.
**Implementation details:** We are single-node (`src/milp/parallel_tree_search.cpp` + `gpu/`); the paper is a roadmap marker that GPU parallelism inside a node and process-level tree parallelism compose into different levels.
**Limitations/failure cases:** Exascale concerns do not apply to our scale; no actionable numbers for a single-GPU workstation.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R8/R7 are single-machine concerns today; this tells us where the architecture would grow (two distinct parallel levels) but changes nothing now.
## Evidence → Engineering Decision
- *Finding:* GPU and tree parallelism are currently evaluated separately (docs/gpu.md crossover study; phase4.json tree scaling) → *PS requirement:* R8 → *Component:* src/milp/parallel_tree_search.cpp → *Metric:* end-to-end speedup with GPU engine per node
## Related Papers
- [[Lin-2025-PDCS-Primal-Dual]]
- [[Lu-2025-cuPDLP-GPU-Implementation]]
## Uses
- [[Parallel Speedup]]
