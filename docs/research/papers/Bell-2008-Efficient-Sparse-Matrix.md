---
type: paper
title: "Efficient Sparse Matrix-Vector Multiplication on CUDA"
authors: "Bell & Garland"
year: 2008
venue: "NVIDIA Technical Report"
doi: "(unverified)"
domain: [gpu]
priority: ○
status: standard
tags: [paper, gpu]
---
# Efficient Sparse Matrix-Vector Multiplication on CUDA

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> The canonical GPU SpMV kernel-design report: format selection, per-row load balancing, and the memory-bound reality behind every CUDA PDHG step.
## Metadata
| Field | Value |
|---|---|
| Authors | Bell & Garland |
| Year | 2008 |
| Venue | NVIDIA Technical Report |
| DOI/URL | (unverified) |

## Problem Addressed
SpMV y = A·x is the inner loop of first-order optimization kernels but has irregular memory access and per-row work. On GPUs, naive CSR kernels idle warps on long rows and stall on memory; the report asks which storage format and thread mapping make SpMV fast.
## Core Contribution
- **Methodology:** Compares CSR (thread-per-row, warp-per-row), ELLPACK-style padding, and HYB (ELL+COO overflow) formats; chooses format from average row length and row-length variance; maps rows to warps for load balance.
- **Assumptions:** y and x fit in device memory; the matrix is reused across many iterations (amortizing format conversion).
- **Benchmarks/datasets:** Sparse matrices from the Sparemeter/SparseBench suites of the era (not re-verified).
- **Metrics:** SpMV GFlop/s and effective memory bandwidth, kernel time, load imbalance.
- **Key results:** CSR+HYB hybrids beat plain CSR on irregular matrices; SpMV is memory-bound, so achievable throughput is a fraction of peak FLOPS (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** CSR SpMV with warp-per-row mapping; ELL/HYB padding for irregular sparsity.
**Techniques:** Format chosen per matrix (nnz per row statistics); avoid per-iteration host↔device transfers; fuse SpMV with vector updates to raise arithmetic intensity.
**Implementation details:** Our `gpu/kernels/spmv.cu` + `gpu/src/csr.cpp` implement CSR SpMV for the PDHG engine; `docs/gpu.md` reports H2D/kernel/D2H split — at SCALE_10000, kernel 22.8 ms of 24.2 ms total, i.e. transfer overhead is amortized only at scale.
**Limitations/failure cases:** On small Netlib instances the kernel is launch-latency dominated: crossover table shows GPU loses to simplex on AFIRO (0.9×) and SC50A (0.7×).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** SpMV is the dominant cost of our GPU engine, and the paper's format-selection rules are actionable inside `spmv.cu` to move the crossover point N* left.
## Evidence → Engineering Decision
- *Finding:* GPU loses to CPU simplex on AFIRO/SC50A but wins from ~SC50B onward (docs/gpu.md §5, evidence/benchmarks/crossover_study.csv) → *PS requirement:* R8 → *Component:* gpu/kernels/spmv.cu → *Metric:* kernel_ms vs. simplex ms per instance
## Related Papers
- [[Lu-2025-cuPDLP-GPU-Implementation]]
- [[Unknown-n.d.-Accelerating-Optimization-Solvers]]
## Uses
- [[GPU CSR SpMV]]
