---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "apps/markov_cero_solve.cpp:35"
  - "apps/cli_options.hpp:34"
verified_on: 2026-09-25
---

# Solve-Pipeline

> End-to-end flow: MPS → canonicalize → presolve/scale → engine dispatch → postsolve → verify → JSON.

## Responsibility
- Describe how `markov-cero-solve` turns a file into a certified result, and which branch each `--engine` value takes.

## Implementation Facts (Observed)
1. CLI parse and open file (apps/markov_cero_solve.cpp:37-49; open failure → exit 8 at :46-49).
2. Parse model: `io::parse_mps(input)` (apps/markov_cero_solve.cpp:71) — [[MPSParser]].
3. `--engine auto` resolution: quadratic objective → `miqp`/`qp`; else discrete → `milp`, continuous → `primal` (apps/markov_cero_solve.cpp:84-90).
4. Dispatch branches: `parallel` → `milp::solve_parallel` (apps/markov_cero_solve.cpp:92-135); `pdlp` → `first_order::solve_pdlp` with `backend = gpu|cpu` (apps/markov_cero_solve.cpp:136-189); `qp` → `qp::make_quadratic_model` + `solve_qp` (apps/markov_cero_solve.cpp:190-238); `milp|miqp` → `milp::solve` (apps/markov_cero_solve.cpp:239-273); otherwise the LP path below.
5. LP path canonicalizes with integrality relaxed: `transform::sparse_canonicalize(model, /*relax_integrality=*/true)` (apps/markov_cero_solve.cpp:275-276) — [[Canonicalizer]].
6. Presolve (if `--presolve`, default on): `presolve::presolve(sparse_canonical, {max_passes})`, early exit on detected infeasible/unbounded (apps/markov_cero_solve.cpp:279-291) — [[Presolve]].
7. Ruiz scaling (if `--scale`, default on): `scale::equilibrate(working_model, {ruiz_iterations})` (apps/markov_cero_solve.cpp:293-301) — [[RuizScaling]].
8. Solve dense canonical LP: `dual::solve` when `--engine dual` (warm basis via `--warm-start`) else `reference::solve` (apps/markov_cero_solve.cpp:311-339); basis captured for `--save-basis` (apps/markov_cero_solve.cpp:333-337, 404-409).
9. Post-solve: `scale::unscale_solution` (apps/markov_cero_solve.cpp:342-345), then `presolve::postsolve(stack, result, sparse_canonical)` (apps/markov_cero_solve.cpp:347-351).
10. Canonical verification: primal residual `‖Ax−b‖∞` and dual reduced-cost check against tolerances (apps/markov_cero_solve.cpp:353-377); failure → `numerical_failure` "canonical witness rejected" (apps/markov_cero_solve.cpp:383-389).
11. Reconstruct original-space solution (`reconstruct_primal`/`reconstruct_objective`) and run [[IndependentVerifiers]] `verify_primal` on the original model (apps/markov_cero_solve.cpp:391-403).
12. Assemble `JsonOutputData`, compute `verified`, emit JSON (apps/markov_cero_solve.cpp:436-479) — [[Solution-JSON-Writer]]; stderr status line + exit code (apps/markov_cero_solve.cpp:492-498).
13. Exception mapping: `MpsError`/`invalid_argument` → invalid_model, `length_error` → resource_limit, other → numerical_failure (apps/markov_cero_solve.cpp:418-434).

## Dependencies
- [[MPSParser]], [[Canonicalizer]], [[Presolve]], [[RuizScaling]], [[RevisedSimplexEngine]], [[DualSimplexEngine]], [[PDLP-Engine]], [[QP-ADMM-Engine]], [[BranchAndCut]], [[ParallelTreeSearch]], [[IndependentVerifiers]], [[Solution-JSON-Writer]]

## Used By
- [[CLI-MarkovCeroSolve]]

## Research Justification
- ([[Presolve]], [[Ruiz Scaling]] as stages)

## Open Questions / Risks
- Parallel/MILP/first-order branches skip steps 5-11's unscale/postsolve path and set `canonical_verified` directly (apps/markov_cero_solve.cpp:121, 259) — Inference: verification depth differs per engine.
- MILP `miqp` engine name routes to `milp::solve`, which internally chooses QP only for continuous models (apps/markov_cero_solve.cpp:239-240, src/milp/milp_solver.cpp:41-70).
