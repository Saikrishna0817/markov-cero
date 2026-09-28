---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/transform/canonicalize.cpp:48"
  - "src/transform/sparse_canonicalize.cpp:119"
verified_on: 2026-09-25
---

# Canonicalizer

> Converts a general `model::Model` into equality-style canonical form (dense and sparse variants).

## Responsibility
- Remove objective-sense sign, variable lower-bound offsets, free variables, inequality senses and box bounds, recording enough metadata to reconstruct the original-space solution.

## Implementation Facts (Observed)
- Two files: dense `canonicalize(model)` (src/transform/canonicalize.cpp, decl include/markov_cero/transform/canonicalize.hpp:24) and sparse `sparse_canonicalize(model, relax_integrality=false)` (src/transform/sparse_canonicalize.cpp:119, decl include/markov_cero/transform/sparse_canonical_model.hpp:27).
- Objective sense folded into `record.objective_sign = ±1` (src/transform/canonicalize.cpp:59; src/transform/sparse_canonicalize.cpp:132).
- Each variable is shifted by a lower-bound offset and split: bounded-below → one +1 column, bounded-above → one −1 column, free → both (src/transform/canonicalize.cpp:70-83).
- Rows: equality → no slack; `≤` row → `+slack`; `≥` row → sign −1 plus slack (src/transform/sparse_canonicalize.cpp:206-220); RHS becomes `sign*(bound − Σ a_ij·offset_j)` (src/transform/sparse_canonicalize.cpp:185-203).
- Finite box bounds are appended as explicit rows with their own slack (src/transform/sparse_canonicalize.cpp:222-235).
- Objective offset accumulated in `long double` then folded with `objective_sign` (src/transform/sparse_canonicalize.cpp:240-250).
- Sparse path transposes the input into row-oriented adjacency first (src/transform/sparse_canonicalize.cpp:163) and assembles CSC from triplets while merging duplicate entries (src/transform/sparse_canonicalize.cpp:252-296).
- `relax_integrality` drops integrality markers before building (src/transform/sparse_canonicalize.cpp:119-121).
- Dense dimension cap `maximum_canonical_dimension = 8192` (src/transform/canonicalize.cpp:8).
- `SparseCanonicalModel::to_dense()` bridges to engines that need dense storage (include/markov_cero/transform/sparse_canonical_model.hpp:24).

## Dependencies
- [[DenseLU]] (`DenseMatrix`), [[SparseBasis-LU]] (`SparseCsc`)

## Used By
- [[Solve-Pipeline]], [[Presolve]], [[BranchAndCut]], [[CutGenerators]], [[PrimalHeuristics]]

## Research Justification
- (canonical forms are covered by [[Revised Simplex]] prerequisites / ADR-M0-01 in docs/decisions)

## Open Questions / Risks
- Reconstructing `x` after offset-splitting relies on `OriginalVariableMap` being faithfully restored by `reconstruct_primal` (include/markov_cero/transform/canonicalize.hpp:6-10); behaviour for variables whose bounds change during presolve is handled by [[Presolve]] stack order (UNVERIFIED edge cases).
