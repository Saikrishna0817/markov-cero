---
type: concept
tags: [techniques, parallel]
status: stable
verified_on: 2026-09-25
---

# Deterministic Reduction

> Fix the order of floating-point summation and the same inputs give the same outputs — the precondition for any credible parallel benchmark.

## Definition
A reduction (sum, max, norm, dot product) computed in parallel is nondeterministic in floating point because the addition order — and therefore the rounding error — varies with thread scheduling. A deterministic reduction imposes a fixed summation order (fixed tree, fixed block partitioning, serial accumulation of per-block partials) so results are bit-reproducible run to run and thread-count to thread-count, at some cost in peak throughput. In GPU kernels this means avoiding atomics whose ordering the hardware controls, or using fixed-segment partial buffers followed by a canonical merge. Determinism of *the algorithm* (branching decisions, cut selection) additionally requires deterministic tie-breaking whenever floating-point values are compared for equality.

## Why It Matters Here
- R16/R15 benchmarking is meaningless if runs are not reproducible: the same instance must yield the same time/gap across runs, otherwise measured differences are noise.
- Observed state: `reduce.cu` exists in the GPU kernel set (gpu/kernels/), but no documented determinism guarantee — Inference: whether reductions are fixed-order is UNVERIFIED; the CPU parallel tree shares pseudo-costs and incumbents under mutexes with no documented ordering guarantee (src/milp/parallel_tree_search.cpp:70-73, 406-410).
- Literature anchor: Schweizer et al. 2018 (deterministic parallel MIP) treats reproducibility as a first-class solver mode, not a debug flag.

## Key Facts / Rules
- Floating-point addition is non-associative ⇒ different order ⇒ different last bits ⇒ possibly different branch/cut decisions.
- Fixed-tree reduction: each thread sums its own block, blocks combine in index order; bit-identical across runs on the same build.
- Cross-thread-count determinism additionally requires no atomic-add ordering dependence and fixed block sizes.
- Trade-off: deterministic modes cost 5-15% throughput class overhead (order-of-magnitude figure from literature, approximate).

## Related
- [[Parallel Speedup]]
- [[Numerical Error]]
- [[Work Stealing]]
- [[Schweizer-2018-Deterministic-Parallel-MIP]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]

## Referenced By

- [[ED-006-load-balanced-parallel-or-demote|research/engineering-decisions/ED-006-load-balanced-parallel-or-demote]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Parallel Efficiency|research/metrics/Parallel Efficiency]]
- [[GPU CSR SpMV|research/techniques/GPU CSR SpMV]]
- [[Work Stealing|research/techniques/Work Stealing]]