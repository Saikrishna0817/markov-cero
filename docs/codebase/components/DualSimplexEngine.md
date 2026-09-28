---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/lp/dual/dual_simplex.cpp:363"
  - "include/markov_cero/lp/dual/dual_simplex.hpp:62"
verified_on: 2026-09-25
---

# DualSimplexEngine

> Warm-start-capable dual simplex used mainly for re-optimization inside branch-and-bound.

## Responsibility
- Run primal-feasible dual-simplex iterations from a stored basis (`BasisState`), producing certified optimal/Farkas outcomes; cold starts delegate to the reference engine.

## Implementation Facts (Observed)
- `solve(model, options, warm_start)` at src/lp/dual/dual_simplex.cpp:363; declaration include/markov_cero/lp/dual/dual_simplex.hpp:62.
- No warm start → `cold(m, o, "cold solve delegated to certified M3 oracle")` (src/lp/dual/dual_simplex.cpp:388-390), i.e. the reference engine is invoked (Inference: "M3 oracle" = [[RevisedSimplexEngine]]).
- Warm basis is validated against a 16-hex-char FNV-style model fingerprint and dimension checks (src/lp/dual/dual_simplex.cpp:123-145, 286); basis file format header `MARKOV-CERO-BASIS-1` (src/lp/dual/dual_simplex.cpp:327).
- Non-dual-feasible warm basis at step 0 either falls back to cold or throws, per `allow_cold_fallback` (default true) (src/lp/dual/dual_simplex.cpp:425-431, include/markov_cero/lp/dual/dual_simplex.hpp:31).
- Ratio test supports Harris ratio (`harris_ratio`, default true); pricing policy `tableau_norm` is explicitly documented as *not* conventional dual steepest-edge (include/markov_cero/lp/dual/dual_simplex.hpp:12-14, 32).
- Condition trigger: if `min|pivot|/max|pivot| < condition_trigger` (default 1e-14) the solve aborts with "basis condition trigger reached" (src/lp/dual/dual_simplex.cpp:405-411).
- Infeasible dual phase yields a Farkas certificate checked by `verify_reference_result` (src/lp/dual/dual_simplex.cpp:271-278).
- Same caps as reference: 1024 rows / 8192 cols / 1e6 iterations (src/lp/dual/dual_simplex.cpp:20-25).
- Refactorization counts reported via `Result::refactorizations` (src/lp/dual/dual_simplex.cpp:472-475).

## Dependencies
- [[SparseBasis-LU]], [[RevisedSimplexEngine]], [[IndependentVerifiers]]

## Used By
- [[Solve-Pipeline]], [[BranchAndCut]], [[ParallelTreeSearch]], [[CLI-MarkovCeroSolve]]

## Research Justification
- [[Dual Simplex]]

## Open Questions / Risks
- Warm-start basis interchange is a custom text format; compatibility with external formats (e.g. CPLEX .bas) is UNVERIFIED.
- `tableau_norm` naming risk: header itself warns it must not be advertised as exact DSE (include/markov_cero/lp/dual/dual_simplex.hpp:12-13).
