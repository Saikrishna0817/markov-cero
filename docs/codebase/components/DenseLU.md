---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/linalg/dense_lu.cpp:45"
  - "include/markov_cero/linalg/dense_lu.hpp:19"
verified_on: 2026-09-25
---

# DenseLU

> Dense row-major LU with partial pivoting and pivot-quality diagnostics.

## Responsibility
- Provide `solve` / `solve_transpose` for dense canonical systems, plus the `DenseMatrix` container used by the dense canonical model.

## Implementation Facts (Observed)
- API: `DenseMatrix` (include/markov_cero/linalg/dense_lu.hpp:5-12), `LuDiagnostics` (hpp:13-18), `DenseLu` class (hpp:19-32); free helpers `multiply`, `multiply_transpose`, `infinity_residual` (hpp:33-37).
- `DenseLu::factorize(matrix, singular_tolerance=1e-14)` (include/markov_cero/linalg/dense_lu.hpp:21); implementation at src/linalg/dense_lu.cpp:44+ performs Gaussian elimination with row interchange recorded in `pivots_` (src/linalg/dense_lu.cpp:54, 71-72).
- Singular / near-singular pivot throws `"singular or near-singular matrix"` (src/linalg/dense_lu.cpp:71).
- `LuDiagnostics` tracks max original entry, min/max |pivot| and `pivot_ratio = min/max` (src/linalg/dense_lu.cpp:77-97).
- Forward/back substitution apply row interchanges in both `solve` and `solve_transpose` (src/linalg/dense_lu.cpp:107-108, 144-145).

## Dependencies
- none (leaf linear-algebra module)

## Used By
- [[RevisedSimplexEngine]] and [[DualSimplexEngine]] include the header; [[Canonicalizer]]'s `CanonicalModel` embeds a `DenseMatrix` (include/markov_cero/transform/canonicalize.hpp:17); [[BranchAndCut]] node LPs convert sparse→dense before solving

## Research Justification
- (dense Gaussian elimination; no dedicated research note)

## Open Questions / Risks
- Inference: `DenseMatrix` usage means every node LP is densified (`to_dense()`), so cost scales with rows×cols — a scaling risk outside the reference caps.
- Interaction of `infinity_residual` with engine certification is exercised through [[IndependentVerifiers]] (UNVERIFIED detail).
