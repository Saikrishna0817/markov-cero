---
type: architecture
tags: [architectures, sparse]
status: stable
verified_on: 2026-09-25
---

# CSC Sparse Model

> The compressed-sparse-column canonical form at the center of the pipeline — one representation that presolve, scaling, factorization and the GPU all consume.

## Definition
A CSC (compressed sparse column) model stores A as three arrays — column pointers, row indices, values — so column operations (pricing, factorization, SpMV by column) are contiguous while row access requires a transpose pass. The canonical `SparseCanonicalModel` here is built from triplets with duplicate merging and carries the metadata (objective sign, offsets, variable maps) needed to reconstruct the original problem after presolve and scaling. CSC is chosen because the consumers that dominate cost — basis LU, column pricing, presolve column scans — are column-oriented; row-oriented consumers (row scans, CSR SpMV on GPU) are served by an explicit transpose at the boundary.

## Why It Matters Here
- R6 (sparse matrix techniques) and R12 (scale) are implemented at this layer first: everything else is a function over this structure.
- Observed state: `sparse_canonicalize` transposes input into row-oriented adjacency, assembles CSC from triplets while merging duplicates (src/transform/sparse_canonicalize.cpp:163-296); presolve, `SparseBasis-LU` and cut extraction all operate on this type; a dense bridge `to_dense()` exists for engines that need dense storage (include/markov_cero/transform/sparse_canonical_model.hpp:24).
- Inference: the dense bridge plus 8192-dimension cap (src/transform/canonicalize.cpp:8) is the boundary where the sparse architecture currently gives way to the reference engines' caps.

## Key Facts / Rules
- CSC arrays: col_ptr (n+1), row_ind (nnz), val (nnz); duplicates merged at assembly time.
- Column op = contiguous walk; row op = transpose or a row-index build — choose consumers accordingly.
- Object storage keeps integrality markers, bounds and objective offsets so integrality can be dropped (`relax_integrality`) without a second parser.
- GPU path converts to CSR (both A and Aᵀ) at upload — CSC is the internal canonical, CSR the device layout.

## Related
- [[Sparsity]]
- [[Multi-Engine Solver Architecture]]
- [[Presolve-Postsolve Stack]]
- [[Canonicalizer]]
- [[SparseBasis-LU]]
- [[Fill-Reducing Ordering]]

## Referenced By

- [[07-current-architecture|audit/07-current-architecture]]
- [[21-traceability|audit/21-traceability]]
- [[Codebase MOC|codebase/Codebase MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Multi-Engine Solver Architecture|research/architectures/Multi-Engine Solver Architecture]]
- [[Presolve-Postsolve Stack|research/architectures/Presolve-Postsolve Stack]]
- [[Presolve|research/concepts/Presolve]]
- [[Sparsity|research/concepts/Sparsity]]
- [[ED-004-sparse-first-canonicalization|research/engineering-decisions/ED-004-sparse-first-canonicalization]]
- [[research-dependency-map|research/maps/research-dependency-map]]