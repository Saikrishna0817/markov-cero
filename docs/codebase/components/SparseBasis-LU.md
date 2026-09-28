---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/linalg/sparse_basis.cpp:81"
  - "include/markov_cero/linalg/sparse_basis.hpp:63"
verified_on: 2026-09-25
---

# SparseBasis-LU

> Sparse LU factorization of the simplex basis with eta-vector (Product-Form-of-Inverse) updates.

## Responsibility
- Factorize a square CSC basis once, then absorb up to `maximum_updates` column replacements as eta vectors, refactoring when density/update limits are hit.

## Implementation Facts (Observed)
- Two classes in include/markov_cero/linalg/sparse_basis.hpp: `SparseLu` (hpp:25) and `SparseBasisFactorization` (hpp:63).
- `SparseLu::factorize` (src/linalg/sparse_basis.cpp:81) converts CSC to sorted row lists, then eliminates column-by-column with **partial pivoting = largest |entry| in the current column** (src/linalg/sparse_basis.cpp:120-135); a pivot ≤ `singular_tolerance` throws `"singular sparse basis"` (src/linalg/sparse_basis.cpp:130-131).
- Diagnostics record min/max |pivot|, factor nonzeros and growth factor (include/markov_cero/linalg/sparse_basis.hpp:17-24, src/linalg/sparse_basis.cpp:140-143).
- `SparseBasisFactorization` keeps a base factor plus a chain of `Eta {pivot, pivot_value, entries}` records applied on each `solve`/`solve_transpose` (include/markov_cero/linalg/sparse_basis.hpp:79-88, src/linalg/sparse_basis.cpp:344-368).
- Options: `singular_tolerance 1e-14`, `update_pivot_tolerance 1e-12`, `eta_density_trigger 0.5`, `maximum_updates 64`, `maximum_dimension 4096`, 4M nnz limits (include/markov_cero/linalg/sparse_basis.hpp:44-52).
- `replace_column` throws `"unstable sparse basis update pivot"` if the update pivot is below `update_pivot_tolerance` (src/linalg/sparse_basis.cpp:403-404); `needs_refactorization()` triggers a full refactor (src/linalg/sparse_basis.cpp:380-381).
- Callers (both simplex engines) tighten `maximum_dimension` to 1024 and cap eta trigger at 0.5 (src/lp/reference/revised_simplex.cpp:65-75, src/lp/dual/dual_simplex.cpp:79-89).

## Dependencies
- none (leaf linear-algebra module)

## Used By
- [[RevisedSimplexEngine]], [[DualSimplexEngine]], [[CutGenerators]] (`extract_basis_matrix`), [[IndependentVerifiers]]

## Research Justification
- [[Sparse LU]]

## Open Questions / Risks
- Pivoting is column-max only (no Markowitz row/column count): Inference: fill-in can blow up on ill-conditioned bases before the nnz cap fires.
- Update-vs-refactor policy constants (64 updates, 0.5 density) are not justified in-code (UNVERIFIED rationale).
