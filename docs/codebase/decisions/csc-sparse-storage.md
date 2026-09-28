---
type: codebase-decision
tags: [codebase, decision, storage, linalg]
status: verified
verified_on: 2026-09-25
evidence:
  - "docs/architecture.md:48-49"
  - "docs/history.md:16"
  - "docs/decisions/ADR-M0-01-canonical-forms.md:3"
---

# CSC Sparse Storage

> Models are immutable compressed-sparse-column arrays; all transformations go through explicit canonicalization.

## Observed Facts
- `docs/architecture.md:48-49`: "**Model Representation (`src/model/model.cpp`)**: Immutable Compressed Sparse Column (CSC) representation with column pointers, row indices, and double-precision coefficients."
- Introduced at M1: `docs/history.md:16` — "Implemented immutable Compressed Sparse Column (CSC) model representation", alongside the strict free-format MPS parser (`docs/history.md:15`).
- `docs/decisions/ADR-M0-01-canonical-forms.md:3` fixes the companion representation choice: explicit row/variable bounds, symbolic infinity, minimization internally, nonnegative bound multipliers, with the consequence that "transformations and postsolve records must preserve model meaning and certificate signs".
- QP reuses the same scheme for its quadratic term: symmetric sparse CSC in `src/qp/model.cpp` (`CHANGELOG.md:6`, "QuadraticModel canonical form with symmetric sparse CSC matrix storage").
- Canonical form conversion lives in `src/transform/canonicalize.cpp` and `src/transform/sparse_canonicalize.cpp` (`docs/architecture.md:51-53`), with a dedicated test `sparse_canonicalize` (`CMakeLists.txt:179`).

## Impact (Inference)
- A single CSC layout across LP/QP keeps the simplex basis code, PDLP matrix-free path, and GPU CSR kernels fed from one structure ([[GPU CSR SpMV]]).
- Immutability means cut injection and presolve must operate on copies/derived models, which is visible in the root-cut path (`src/milp/milp_solver.cpp:172-183`).

## Related
- [[no-external-solver-dependency]] · [[status-certificate-fail-closed]] · [[mps-parser-limitations]]
