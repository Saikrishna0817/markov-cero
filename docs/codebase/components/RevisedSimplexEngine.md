---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/lp/reference/revised_simplex.cpp:383"
  - "include/markov_cero/lp/reference/revised_simplex.hpp:49"
verified_on: 2026-09-25
---

# RevisedSimplexEngine

> Certified two-phase primal revised simplex reference engine for canonical LPs.

## Responsibility
- Solve `transform::CanonicalModel` LPs with a dense work matrix, a maintained basis factorization, and post-solve certification of every terminal result.

## Implementation Facts (Observed)
- Entry point `solve(model, options)` at src/lp/reference/revised_simplex.cpp:383, declared at include/markov_cero/lp/reference/revised_simplex.hpp:49.
- Hard caps: 1024 rows, 8192 columns, 4M expanded elements, 1,000,000 iterations, tolerance ≤1e-4 (src/lp/reference/revised_simplex.cpp:15-20).
- Work matrix is dense row-major with row-sign normalization and appended identity/slack columns (`make_work`, src/lp/reference/revised_simplex.cpp:247-274).
- Phase-I artificials are added unless `crash_basis` succeeds (src/lp/reference/revised_simplex.cpp:403, 276); phase-I dual becomes a Farkas certificate when the phase-I objective stays positive (src/lp/reference/revised_simplex.cpp:426-434).
- Pricing: first negative reduced cost when `bland_anti_cycling` (default `true`, include/markov_cero/lp/reference/revised_simplex.hpp:23), else most-negative (src/lp/reference/revised_simplex.cpp:128-151); ratio-test ties break on smallest basis index (src/lp/reference/revised_simplex.cpp:154-173).
- Basis maintained by `linalg::SparseBasisFactorization` with eta updates; `maximum_updates=64`, `eta_density_trigger=0.5` (src/lp/reference/revised_simplex.cpp:65-75), refactorized on demand or after update failure (src/lp/reference/revised_simplex.cpp:236-242).
- Every terminal result goes through `verify::verify_reference_result` inside `certify` (src/lp/reference/revised_simplex.cpp:357).

## Dependencies
- [[SparseBasis-LU]], [[DenseLU]], [[Canonicalizer]], [[IndependentVerifiers]]

## Used By
- [[Solve-Pipeline]], [[CLI-MarkovCeroSolve]], [[BranchAndCut]], [[PrimalHeuristics]]

## Research Justification
- [[Revised Simplex]]

## Open Questions / Risks
- Dense 4M-element workspace cap and 1024-row cap: Inference: these caps are why the module is labelled "reference" rather than production-scale.
- Bland pricing default trades speed for anti-cycling; impact on large models not measured here (UNVERIFIED).
