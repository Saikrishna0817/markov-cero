---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/presolve/presolve.cpp:20"
  - "include/markov_cero/presolve/presolve.hpp:39"
verified_on: 2026-09-25
---

# Presolve

> Bound-propagation presolve with an explicit, reversible reduction stack and matching postsolve.

## Responsibility
- Repeatedly remove empty rows/columns, fix variables and substitute row singletons on a `SparseCanonicalModel`, then restore a full-space solution via `postsolve`.

## Implementation Facts (Observed)
- API `presolve(input, options)` and `postsolve(stack, reduced_solution, original_model, tolerance=1e-8)` (include/markov_cero/presolve/presolve.hpp:39-45).
- `PresolveOptions`: `max_passes 5`, feasibility 1e-9, dual 1e-9, pivot 1e-12 (include/markov_cero/presolve/presolve.hpp:12-17); CLI default 5 passes (apps/cli_options.hpp:22, `--max-presolve-passes`).
- Four reduction types tracked in `ReductionRecord = variant<EmptyRow, EmptyColumn, FixedVariable, RowSingleton>` (include/markov_cero/presolve/presolve_stack.hpp:35-36).
- Pass body: scan empty rows (src/presolve/presolve.cpp:67), empty columns (src/presolve/presolve.cpp:93), row singletons (src/presolve/presolve.cpp:119), then fix-and-substitute variables (src/presolve/presolve.cpp:153), compact active rows/cols (src/presolve/presolve.cpp:177).
- Statistics count each reduction class plus original/presolved dimensions (include/markov_cero/presolve/presolve.hpp:19-29).
- Early exit when reduced model detects infeasible/unbounded (src/lp dispatch in apps/markov_cero_solve.cpp:283-287).
- `postsolve` copies un-eliminated values, then pops records in **LIFO** order (src/presolve/presolve.cpp:255, 270), solving fixed-variable dual values via `pi_i = (c_k - Σ a_rk pi_r)/a_ik` (src/presolve/presolve.cpp:283-284) and recomputing the exact objective in original canonical space (src/presolve/presolve.cpp:308).

## Dependencies
- [[Canonicalizer]] (`SparseCanonicalModel`), [[IndependentVerifiers]]

## Used By
- [[Solve-Pipeline]] (apps/markov_cero_solve.cpp:279-291, 347-351), [[CLI-MarkovCeroSolve]] (`--presolve/--no-presolve`)

## Research Justification
- [[Presolve]]

## Open Questions / Risks
- Only four reductions implemented; no implied-bound / coefficient-based tightening observed (fact of absence in src/presolve/presolve.cpp:67-177).
- Postsolve dual reconstruction correctness for chained fixed variables is asserted by tests (existence of presolve_test) but UNVERIFIED here.
