---
type: concept
tags: [techniques, gpu]
status: stable
verified_on: 2026-09-25
---

# GPU CSR SpMV

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> y = A·x with A in compressed-sparse-row on the device — the one kernel every first-order GPU method lives or dies on.

## Definition
Sparse matrix-vector multiplication in CSR format stores values contiguously per row (`row_ptr`, `col_ind`, `val`) so each thread (or warp) owns a row and accumulates dot products with coalesced access to the value array. CSR is the natural format for A·x; Aᵀ·y needs either a second format (CSC/CSR-of-Aᵀ) or an atomic-accumulate pass, which is why GPU LP engines upload both A and Aᵀ. The kernel is memory-bandwidth-bound: arithmetic intensity is ~1 flop per nonzero, so achieved performance is measured against the device's bandwidth roofline, and the design goal is to keep iterates resident on-device so SpMV is never dominated by PCIe transfers.

## Why It Matters Here
- R8 allows GPU only "where it provides measurable benefits" — SpMV dominates PDHG iteration cost, so this kernel determines whether the claim can be made.
- Observed state: kernels `spmv.cu`, `reduce.cu`, `vector_ops.cu` exist under gpu/kernels/; A and Aᵀ are uploaded as CSR once (gpu/src/pdhg_step.cpp:391-395) and iteration chunks run with no per-iteration host transfer (gpu/src/pdhg_step.cpp:434-443).
- Observed state: docs/gpu.md reports kernel ≈22.8 ms of 24.2 ms total at SCALE_10000 — Inference: transfer is already amortized, so remaining headroom is kernel efficiency and iteration count, not data movement.

## Key Facts / Rules
- CSR SpMV cost O(nnz); performance bound by memory bandwidth (Bell & Garland 2008 gives the canonical CUDA design study).
- Aᵀ·y requires the transpose structure — hence storing both directions on device.
- Load imbalance: rows with wildly different nonzero counts skew thread work; row-block partitioning mitigates it.
- CSR is fixed-size: inserting rows (cut generation) requires rebuilding the arrays — a host-side concern.

## Related
- [[Sparsity]]
- [[Primal-Dual Hybrid Gradient]]
- [[GPU Benefit Unproven]]
- [[Deterministic Reduction]]
- [[Bell-2008-Efficient-Sparse-Matrix]]
- [[Lu-2025-cuPDLP-GPU-Implementation]]

## Referenced By

- 15-roadmap
- csc-sparse-storage
- Architecture MOC
- Research MOC
- [[Primal-Dual Hybrid Gradient|research/algorithms/Primal-Dual Hybrid Gradient]]
- [[Sparsity|research/concepts/Sparsity]]
- [[ED-007-honest-gpu-scoping|research/engineering-decisions/ED-007-honest-gpu-scoping]]
- [[Bell-2008-Efficient-Sparse-Matrix|research/papers/Bell-2008-Efficient-Sparse-Matrix]]
- [[Unknown-n.d.-Accelerating-Optimization-Solvers|research/papers/Unknown-n.d.-Accelerating-Optimization-Solvers]]
- [[GPU Benefit Unproven|research/research-gaps/GPU Benefit Unproven]]
- [[Adaptive Restart|research/techniques/Adaptive Restart]]