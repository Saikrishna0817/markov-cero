---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/milp/gomory.cpp:12"
  - "src/milp/mir.cpp:21"
verified_on: 2026-09-25
---

# CutGenerators

> Gomory mixed-integer and mixed-integer-rounding cut generation from the root LP basis, plus shared cut filtering.

## Responsibility
- Extract the basis, form tableau rows for fractional basic variables, build GMI/MIR inequalities in *original* variable space, and keep only a small non-redundant subset.

## Implementation Facts (Observed)
- GMI: `generate_gomory_cuts(model, original_primal, canonical, basis_state, max_cuts, min_fractionality)` at src/milp/gomory.cpp:12; skips rows with `f0` outside `[min_fractionality, 1−min_fractionality]` (src/milp/gomory.cpp:96); tableau row `ā = y^T A` (src/milp/gomory.cpp:110-113); coefficients mapped back through `OriginalVariableMap` to original columns (src/milp/gomory.cpp:175-189); a cut is kept only if it has nonzeros and cuts the incumbent point (src/milp/gomory.cpp:202-213).
- MIR: `generate_mir_cuts(...)` at src/milp/mir.cpp:21, with `mir_function(a, f0)` (src/milp/mir.cpp:12) applied in **direct and complement orientations, citing Marchand & Wolsey 2001** (src/milp/mir.cpp:121-141); violation threshold 1e-4 (src/milp/mir.cpp:222-223).
- Both generators return through `filter_cuts` (src/milp/gomory.cpp:219, src/milp/mir.cpp:228).
- `filter_cuts(candidates, max_cuts=10, min_violation=1e-4, max_parallelism=0.95)` sorts by efficacy, drops invalid/too-small cuts, and rejects candidates whose cosine similarity to an accepted cut exceeds `max_parallelism` (include/markov_cero/milp/cut_pool.hpp:22-24, src/milp/cut_pool.cpp:62-100).
- `Cut` is `{coefficients, rhs, violation}` representing `Σ c_j x_j ≥ rhs` (include/markov_cero/milp/cut_pool.hpp:12-16); `add_cuts_to_model` appends rows to the model (include/markov_cero/milp/cut_pool.hpp:26).
- Requires a `BasisState`; generation is skipped when the root LP has no basis (src/milp/milp_solver.cpp:170).
- Cuts are injected at the **root only**, then root LP re-solved once (src/milp/milp_solver.cpp:174-205); the same root-only pattern is repeated in src/milp/parallel_tree_search.cpp:339-346.
- Counts reported as `Result::cuts_generated` (src/milp/milp_solver.cpp:184).

## Dependencies
- [[Canonicalizer]], [[SparseBasis-LU]] (`extract_basis_matrix`), [[DualSimplexEngine]] (`BasisState`)

## Used By
- [[BranchAndCut]], [[ParallelTreeSearch]], [[CLI-MarkovCeroSolve]] (`--cuts/--no-cuts`)

## Research Justification
- [[Gomory Mixed Integer Cut]], [[Mixed Integer Rounding Cut]]

## Open Questions / Risks
- `min_fractionality` default value and cut-loop depth are passed as `options.max_cut_rounds` from the caller (src/milp/milp_solver.cpp:174-178); the generator does not iterate additional rounds itself (fact).
- No cut validity re-verification against the LP relaxation beyond the violation test (observed in src/milp/gomory.cpp:202-213).
