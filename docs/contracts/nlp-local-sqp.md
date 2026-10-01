# Local SQP and callback contract (v1)

Binding contract for how the NLP path validates callbacks, diagnoses
derivatives, qualifies a local first-order point, and reports statuses and
incumbents. It closes blueprint task NLP-01 (blueprint §14): NLP is
advertised as a **local SQP solver** with documented limits; global
nonconvex optimization stays out of scope.

Related contracts and evidence:

- [Numerical contract](numerical-policy.md) — tolerance inventory (§5.6),
  assurance derivation (`local_kkt_checked`) and status witness table (§3).
- [Resource limits contract](resource-limits.md) — deadline stops and
  cooperative polling for callback and solve stages.
- [Convex QP contract](convex-qp.md) — the QP subproblem solver the SQP step
  uses (`qp::solve_qp`).
- `evidence/INDEX.md` — benchmark records for the §7 corpus obligation.

Algorithmic reference: Nocedal–Wright, *Numerical Optimization*, SQP chapter.
Local research notes are secondary.

## 1. Supported class and the exact guarantee

1. **Class.** Continuously differentiable `f`, `g ≤ 0`, `h = 0` over the
   evaluated domain, with finite returned values, solved by SQP with an
   L-BFGS positive-definite curvature model, an l1 merit function and a
   bounded step (`nlp::SqpSolver`, `SqpOptions`).
2. **Guarantee.** A reported result is a **verified first-order KKT
   candidate**: primal feasibility, stationarity in the bound-normal cone,
   inequality multiplier sign and complementarity all pass the independent
   checker (`nlp::verify_nlp_solution`) at the stated tolerance. First-order
   KKT is necessary under constraint qualification; it does **not** prove a
   local minimum (a saddle satisfies it) and never proves globality.
3. **Status.** The JSON status of such a point is `LocalStationary`
   (contract [numerical-policy](numerical-policy.md) v2; the C++ enum
   `local_optimal` is retained for source compatibility and only its public
   spelling changed). `local_minimum_verified` is reserved for a future
   second-order sufficiency test and is not emitted. `Optimal` is never
   emitted on the NLP path.
4. **What is never claimed.** SQP failure is never a proof of infeasibility;
   agreement across multiple starts never implies globality; KKT acceptance
   never implies second-order sufficiency.

## 2. Callback contract (validated at every evaluation)

Every objective, gradient, constraint-value and Jacobian evaluation during
`solve_sqp` runs through checked adapters (`src/nlp/nlp_callback_guard.*`).
A rejected evaluation is **fail-closed**: the solve stops with
`NumericalFailure` and a message naming the callback; it never produces
`LocalStationary` or `Optimal`.

1. **Dimensions.** `gradient` returns exactly `n_vars` values;
   `ineq_constraints`/`eq_constraints` return exactly `n_ineq`/`n_eq`
   values; each Jacobian has exactly `n_ineq`/`n_eq` rows and every row has
   exactly `n_vars` columns. Counts may not change between evaluations
   during one solve.
2. **Finiteness.** Every returned scalar is finite (no NaN, no ±inf).
3. **Exceptions.** `std::exception` and non-standard exceptions from any
   callback are caught at the solver boundary and mapped to the
   fail-closed `NumericalFailure` above; the message carries the
   exception text so the failing callback is identifiable.
4. **Counters.** The solver counts callback evaluations
   (`SqpSolution::callback_evaluations`) and exposes the count through the
   C++ result and the Python binding so cost is observable (blueprint
   work sequence 7).
5. **Stalling.** A callback that never returns is only bounded by the
   cooperative deadline polls at major-iteration boundaries; hosted
   deployments must use subprocess isolation (resource-limits/hosted
   contracts). The local library documents this limit rather than
   pretending to preempt user code.
6. **Scope.** Validation covers the solve path. The independent verifier
   (`verify_nlp_feasibility` / `verify_nlp_solution`) keeps its own
   fail-closed checks (dimension, finiteness, exception) so a verification
   call on raw user callbacks is safe outside a solve.

## 3. Derivative diagnostic (developer-only)

`nlp::check_derivatives(model, x, step_hint)` reports, per the model at a
point:

1. **Gradient check.** Each analytic gradient component is compared with a
   centered finite difference of the objective in the interior, and a
   one-sided difference within one step of a finite bound. The step is
   `h_j = cbrt(machine epsilon) * max(1, |x_j|)` scaled by `step_hint`
   when given.
2. **Jacobian check.** Each analytic constraint Jacobian row is compared
   with centered/one-sided finite differences of the corresponding
   constraint values under the same step rule.
3. **Reporting.** The report carries the worst absolute error per block,
   the step rule used, and a pass/fail verdict against a documented error
   threshold (`1e-5` absolute + `1e-4` relative to the local scale).
4. **Role.** The diagnostic **diagnoses user callbacks only**. It is never
   called inside the production solve path and never silently replaces
   analytic derivatives. A failing diagnostic does not change a solve
   status; it is a developer tool.

## 4. SQP iteration semantics (audited in NLP-01)

1. **Step model.** Each major iteration solves the convex QP of
   `sqp_solver_subproblem.cpp`: `½dᵀB_kd + ∇fᵀd` subject to the linearized
   `g + J_g d ≤ 0`, `h + J_h d = 0` and bound rows `l − x ≤ d ≤ u − x`,
   with `B_k` the positive-definite L-BFGS model (curvature pairs with
   non-positive `sᵀy` are discarded, `lbfgs.cpp`).
2. **Trust cap.** The QP step is capped at `kTrustRadius = 0.5` in the
   ∞-norm before the line search (documented bound-constrained SQP
   globalization; without it ill-conditioned objectives stall).
3. **Merit and line search.** An l1 merit with penalty `mu`; descent is
   validated before searching. The bracketed search enforces Armijo and
   the Wolfe curvature condition **with a documented waiver**: the l1 merit
   is nonsmooth, so strict Wolfe is not always attainable — inside the
   2× expansion cap the best Armijo-satisfying step is accepted
   (`sqp_solver.cpp`). "Strong Wolfe always holds" is not claimed.
4. **Fallback.** A failed QP subproblem resets `B_k` to identity, escalates
   `mu`, and restarts from the **current** point, at most
   `max_hessian_resets` (3) times. A QP subproblem reported
   `primal_infeasible` is a different case: `B_k` affects only the
   objective, so the linearized constraint set cannot become feasible by
   resetting — the solve fails immediately with `NumericalFailure` and a
   message naming the linearized-infeasibility. Either way the outcome is
   inconclusive: never `Infeasible`, never a claim that the NLP is
   infeasible (restoration/elastic subproblems are the deferred NLP-02
   work).
5. **Initial point.** `x0` is projected onto the variable bounds before the
   first iteration. The projection is reported: `SqpSolution::x0_projected`
   and `x0_projection_norm` (max coordinate movement), and the solve
   message notes a projection larger than 0.
6. **Convergence gate.** `kkt_tolerance` (1e-6) binds feasibility,
   bound-normal stationarity and complementarity at the iterate with the
   current multiplier estimates; acceptance is then re-derived
   independently by the verifier before any status upgrade (§5).

## 5. Status, verification and incumbent rules

1. **Engine gate** (`src/api/engine_nonlinear.cpp`). Only a solver status
   of `optimal` *and* an accepted `verify_nlp_solution` at 1e-6 publish
   `LocalStationary` with `certificate_type = local_kkt` and
   `assurance = local_kkt_checked` (numerical-policy §2). A rejected
   verifier downgrades to `NumericalFailure` with
   `failure_site = nlp_kkt_verification`.
2. **Independent recomputation.** The verifier recomputes feasibility,
   the objective, the gradient/Jacobian values, the bound-normal
   stationarity projection, the multiplier sign and complementarity from
   the original callbacks. The multipliers themselves are the solver's
   estimates (documented asymmetry: recomputing multipliers is not part of
   first-order checking).
3. **Limits keep honest statuses.** Deadline, iteration and reset stops
   return `ResourceLimit`, `IterationLimit` or `NumericalFailure`
   exactly as the stop that occurred; they are never converted to
   `LocalStationary` or `Optimal`.
4. **Best verified feasible point.** On any non-KKT exit (limit, failure),
   if any returned iterate satisfied the feasibility tolerance — including
   the final one — the solver returns the last such iterate in
   `SqpSolution::best_feasible_x` (with its objective), and the engine —
   only after its own `verify_nlp_feasibility` accepts it — attaches it as
   `result.primal` / `original_primal` while keeping the limit/failure
   status. Verification happens before the shared-deadline poll at the end
   of the engine's SQP stage (verification itself is deadline-free,
   resource-limits §4), so a deadline-limited solve still publishes a
   verified incumbent. A feasible incumbent never upgrades a status, and an
   unverifiable point is never attached.
5. **Never on this path.** `Optimal`, `Infeasible` (as a global claim),
   `GapSatisfied`, or any assurance above `local_kkt_checked` for an NLP
   result.

## 6. Test obligations (blueprint §14 cases)

The suites must cover, each as a named check:

1. Unconstrained convex quadratic with known minimizer (existing
   `nlp_sqp`).
2. Rosenbrock from **at least two starts**, reporting local outcome and
   iterations, with no globality inference (`nlp_local_semantics`).
3. Equality-constrained quadratic with the analytic multiplier checked
   against the solver's estimate (`nlp_local_semantics`).
4. Active lower/upper bounds and a changing active set with multiplier
   sign checks through the bound-normal projection (existing
   `nlp_constrained` plus `nlp_local_semantics`).
5. Deliberately wrong gradient/Jacobian callbacks: the derivative
   diagnostic detects the mismatch; correct callbacks pass
   (`nlp_derivative_check`).
6. Saddle `f = x² − y²` at `(0, 0)`: the solve may report a first-order
   point but the public status must be `LocalStationary`, never
   `Optimal`, and the saddle must not be described as a
   verified local minimum (`nlp_local_semantics`).
7. Infeasible constraints `x ≤ 0`, `x ≥ 1`: statuses stay in
   {`NumericalFailure`, `IterationLimit`, `ResourceLimit`} — never
   `Optimal`, `LocalStationary`, or a globally proved infeasibility
   (`nlp_local_semantics`).
8. Callback returns NaN, wrong-sized vector/matrix, or throws: bounded
   fail-closed `NumericalFailure` with a naming message, no crash
   (`nlp_callback_guard_test`).
9. Limit paths keep the honest status and, when one exists, a
   verified feasible incumbent (§5.4) (`nlp_callback_guard_test`).

## 7. Benchmark obligation

A **separate local NLP corpus** (own record under `evidence/`, indexed from
`evidence/INDEX.md`, never merged with the QP/MIP/MIQP summaries) runs the
cases above through the public entry points and records, per case:
start point(s), status as measured, feasibility and stationarity
residuals, iterations, callback evaluations and wall time. Comparison is on
feasibility/stationarity/time only — **no global objective proof, no speed
claim, no cross-solver ranking**.

## 8. Change procedure

Status, assurance-label and witness changes follow
[numerical-policy](numerical-policy.md) §8 (version bump + migration note).
Tolerance changes require the boundary tests
(`numerical_policy_boundary_test`, `assurance_label_test`,
`nlp` acceptance checks) to be re-run with an error analysis. Restoring
inconsistent linearizations (elastic subproblems) is NLP-02 work and must
not land here under the guise of a message fix.
