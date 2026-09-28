---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/scale/ruiz_scaling.cpp:20"
  - "include/markov_cero/scale/ruiz_scaling.hpp:28"
verified_on: 2026-09-25
---

# RuizScaling

> Iterative Ruiz–Torres equilibration (2-norm row/column scaling) with exact solution unscaling.

## Responsibility
- Apply `D_R A D_C` scaling to a sparse or dense model until row/column norms are balanced, and invert the scaling on primal/dual/ray/certificate outputs.

## Implementation Facts (Observed)
- Two entry points: `equilibrate(SparseCanonicalModel&, RuizOptions)` and `equilibrate_model(model::Model&, RuizOptions)` (include/markov_cero/scale/ruiz_scaling.hpp:28-34) — the former is used by the CLI pipeline, the latter by [[PDLP-Engine]] (src/lp/first_order/pdlp.cpp:189-190) and [[GPU-PDHG-Engine]] (gpu/src/pdhg_step.cpp:371-373).
- Options: `max_iterations 10`, `tolerance 1e-3`, `min_scale 1e-4`, `max_scale 1e4` (include/markov_cero/scale/ruiz_scaling.hpp:12-17); CLI exposes `--ruiz-iterations` (default 10, apps/cli_options.hpp:23, 198-204).
- Per-iteration update: compute row and column norms, then `delta_r = 1/sqrt(r_norm)`, `delta_c = 1/sqrt(c_norm)` (src/scale/ruiz_scaling.cpp:82-90); convergence declared when both max norm errors < tolerance (src/scale/ruiz_scaling.cpp:77-79).
- Result struct `RuizScalers` stores `row_scale/col_scale` and inverses plus `iterations_executed`/`converged` (include/markov_cero/scale/ruiz_scaling.hpp:19-26).
- `unscale_solution` rescales primal by `col_scale`, dual by `row_scale`, and also rays and Farkas certificates (src/scale/ruiz_scaling.cpp:129-145).
- Equivalent loop exists for `model::Model` storage layout (src/scale/ruiz_scaling.cpp:167-210).

## Dependencies
- [[Canonicalizer]] (`SparseCanonicalModel`)

## Used By
- [[Solve-Pipeline]] (apps/markov_cero_solve.cpp:293-301, unscale at 342-345), [[PDLP-Engine]], [[GPU-PDHG-Engine]], [[CLI-MarkovCeroSolve]]

## Research Justification
- [[Ruiz Scaling]]

## Open Questions / Risks
- Scaling is applied only for the `primal`/`pdlp` CLI branches; MILP/QP/parallel branches never scale (observed: no `equilibrate` call in src/milp/*).
- `converged` flag is recorded but not checked by the CLI caller (observed at apps/markov_cero_solve.cpp:297-301).
