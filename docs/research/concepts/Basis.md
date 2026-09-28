---
type: concept
tags: [concepts, lp]
status: stable
verified_on: 2026-09-25
---

# Basis

> m linearly independent columns of A — the coordinate system the simplex lives in, and the object every warm start hands to the next solve.

## Definition
For a canonical LP with rank m, a basis is an m×m nonsingular submatrix B of the columns of A; the corresponding basic variables solve x_B = B⁻¹b while all nonbasic variables sit at zero (or a bound). The revised simplex never forms B⁻¹ explicitly — it factorizes B (sparse LU) and applies forward/backward triangular solves (FTRAN/BTRAN) to obtain x_B and the duals πᵀ = c_BᵀB⁻¹. The set of bases is finite, which is what makes simplex finite in exact arithmetic; a basis is degenerate when B⁻¹b has zero entries and singular when B is rank-deficient. Bases are maintained by update (eta/Product-Form-Of-Inverse vectors) and periodically refactorized when fill or update count grows.

## Why It Matters Here
- R6 (sparse matrix techniques, numerical linear algebra) is implemented first and most concretely in basis handling; R4 makes it the core data structure of the LP engines.
- Observed state: `SparseBasisFactorization` keeps a base factor plus a chain of `Eta` records, with `maximum_updates=64`, `eta_density_trigger=0.5`, refactorization on demand (src/linalg/sparse_basis.cpp:344-404; src/lp/reference/revised_simplex.cpp:65-75).
- Observed state: the dual engine persists a warm basis as `BasisState` with header `MARKOV-CERO-BASIS-1`, validated against a 16-hex model fingerprint (src/lp/dual/dual_simplex.cpp:123-145, 327).

## Key Facts / Rules
- x_B = B⁻¹b; π = B⁻ᵀc_B; basic columns have dⱼ = 0 by construction.
- Update vs refactor: rank-one eta updates are cheap but accumulate fill and rounding error — refactorization restores both (Forrest–Tomlin 1972).
- A basis must stay nonsingular; pivots below `singular_tolerance` (1e-14) abort the factorization.
- Number of distinct bases is finite ⇒ no cycling in exact arithmetic; degeneracy is what breaks this in practice.

## Related
- [[Basic Solution]]
- [[Warm Start]]
- [[Sparse LU]]
- [[Revised Simplex]]
- [[SparseBasis-LU]]
- [[Bartels-1969-Simplex-LU-Decomposition]]

## Referenced By

- [[Research MOC|research/Research MOC]]
- [[Revised Simplex|research/algorithms/Revised Simplex]]
- [[Sparse LU|research/algorithms/Sparse LU]]
- [[Basic Solution|research/concepts/Basic Solution]]
- [[Crossover|research/concepts/Crossover]]
- [[Reduced Cost|research/concepts/Reduced Cost]]
- [[Warm Start|research/concepts/Warm Start]]
- [[Achterberg-0000-Objective-Feasibility-Pump|research/papers/Achterberg-0000-Objective-Feasibility-Pump]]
- [[Azulay-0000-Revised-Simplex-Method|research/papers/Azulay-0000-Revised-Simplex-Method]]
- [[Bartels-1968-Numerical-Investigation-Simplex|research/papers/Bartels-1968-Numerical-Investigation-Simplex]]
- [[Bartels-1969-Simplex-LU-Decomposition|research/papers/Bartels-1969-Simplex-LU-Decomposition]]
- [[Berthold-2013-Cloud-Branching|research/papers/Berthold-2013-Cloud-Branching]]
- [[Bixby-1994-Reduced-Cost-Fixing|research/papers/Bixby-1994-Reduced-Cost-Fixing]]
- [[Cline-1979-Estimate-Condition-Number|research/papers/Cline-1979-Estimate-Condition-Number]]
- [[Dantzig-1954-Product-Form-Inverse|research/papers/Dantzig-1954-Product-Form-Inverse]]
- [[Forrest-1972-Updating-Triangular-Factors|research/papers/Forrest-1972-Updating-Triangular-Factors]]
- [[Georg-1987-Numerical-Stability-Simplex|research/papers/Georg-1987-Numerical-Stability-Simplex]]
- [[Gill-1974-Methods-Modifying-Matrix|research/papers/Gill-1974-Methods-Modifying-Matrix]]
- [[Gleixner-2012-Factorization-Update-Reduced|research/papers/Gleixner-2012-Factorization-Update-Reduced]]
- [[Moler-1967-Rounding-Errors-Algebraic|research/papers/Moler-1967-Rounding-Errors-Algebraic]]
- [[Ogryczak-1987-Numerical-Stability-Simplex|research/papers/Ogryczak-1987-Numerical-Stability-Simplex]]
- [[Rader-0000-Selection-Variables-MIP|research/papers/Rader-0000-Selection-Variables-MIP]]
- [[Reid-1982-Sparsity-Exploiting-Variant|research/papers/Reid-1982-Sparsity-Exploiting-Variant]]
- [[Richard-2010-Group-Approach-Cutting|research/papers/Richard-2010-Group-Approach-Cutting]]
- [[Ye-1998-Crossover-Interior-Point|research/papers/Ye-1998-Crossover-Interior-Point]]