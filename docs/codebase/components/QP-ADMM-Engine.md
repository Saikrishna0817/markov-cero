---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/qp/admm_solver.cpp:84"
  - "include/markov_cero/qp/admm_solver.hpp:52"
verified_on: 2026-09-25
---

# QP-ADMM-Engine

> OSQP-style ADMM solver for convex QP (and MIQP node relaxations) with adaptive ρ and infeasibility certificates.

## Responsibility
- Solve `QuadraticModel` (min ½xᵀPx + qᵀx s.t. l ≤ Ax ≤ u) by splitting into primal/dual updates backed by a quasi-definite KKT factorization.

## Implementation Facts (Observed)
- `AdmmQpSolver::solve` at src/qp/admm_solver.cpp:84; free function `solve_qp` at src/qp/admm_solver.cpp:338 (decl include/markov_cero/qp/admm_solver.hpp:52-63).
- Defaults (include/markov_cero/qp/admm_solver.hpp:25-38): `absolute/relative_tolerance 1e-4`, `primal/dual_infeasible_tolerance 1e-5`, `sigma 1e-6`, `rho_init 0.1`, `alpha 1.6`, `max_iterations 4000`, `time_limit_seconds 60`, `adaptive_rho true`, `adaptive_rho_interval 25`.
- KKT factorization built once before the loop via `KktSolver::factorize` (src/qp/admm_solver.cpp:109-112).
- Iteration = assemble rhs → KKT solve (`x_tilde`, `nu`) → over-relaxation with `alpha` (`x_hat = α x̃ + (1−α) x`) → projection of `z` onto box → dual ascent `y += ρ(ẑ − z)` (src/qp/admm_solver.cpp:127-176).
- Convergence uses absolute+relative primal and dual residual bounds scaled by `max(‖Ax‖,‖z‖)` / `max(‖Px‖,‖Aᵀy‖,‖q‖)` (src/qp/admm_solver.cpp:200-210).
- Infeasibility certificates follow **Banjac et al. 2019** (comment src/qp/admm_solver.cpp:212), checked every 10 iterations, setting `primal_infeasible` (src/qp/admm_solver.cpp:223-244) or `dual_infeasible` (src/qp/admm_solver.cpp:263-280).
- Adaptive ρ rescales per-constraint ρ into [1e-6,1e6] every `adaptive_rho_interval` iterations and re-factorizes numerically via `KktSolver::update_numeric` (src/qp/admm_solver.cpp:288-303).
- Convexity gate: `QpStatus::non_convex` is returned from model checking (src/qp/admm_solver.cpp:89), driven by `check_convexity` in src/qp/model.cpp:142-206 (diagonal screen + dense LDLᵀ diagonal elimination).
- CLI wires `qp`/`miqp` engines here: `qp` → direct `solve_qp` (apps/markov_cero_solve.cpp:190-238); `miqp` → [[BranchAndCut]] node relaxations (src/milp/node_lp.cpp:15-38).
- CLI QP tolerances are hardcoded 1e-5 and `max_iterations` default 4000 (apps/markov_cero_solve.cpp:192-198).

## Dependencies
- [[LDL-Factorization]] (`KktSolver`), [[Canonicalizer]] (`make_quadratic_model`), [[IndependentVerifiers]]

## Used By
- [[Solve-Pipeline]] (`qp`/`miqp`), [[BranchAndCut]], [[CLI-MarkovCeroSolve]]

## Research Justification
- [[ADMM]], [[Interior-Point Method]] (contrast only — this is a first-order splitting method)

## Open Questions / Risks
- Statuses `iteration_limit`/`time_limit` are not distinguished from failure in the CLI branch (apps/markov_cero_solve.cpp:234-238).
- Non-convex models are rejected rather than solved (src/qp/model.cpp:142-206); `SparseSymmetricMatrix` dimension capped at 4096 (src/qp/model.cpp:11-12).
