# Elastic SQP restoration contract (v1)

Binding contract for blueprint task NLP-02: what happens when the SQP
linearized subproblem is primal-infeasible, the exact penalty and
acceptance rules, the boundedness argument, diagnostics, and the frozen
failure corpus this work was measured against before implementation. It
closes the NLP-02 deferral in [nlp-local-sqp](nlp-local-sqp.md) §4.4.

Related contracts and evidence:

- [Local SQP contract](nlp-local-sqp.md) — the iteration semantics this
  path extends (§4), status/incumbent rules it never bypasses (§5).
- [Convex QP contract](convex-qp.md) — `qp::solve_qp` solves both the
  standard and the elastic subproblem.
- [Numerical contract](numerical-policy.md) — status witness table (§3)
  and the change procedure (§8) followed here.
- `evidence/INDEX.md` — the paired §8 benchmark record.

Algorithmic reference: Nocedal–Wright, *Numerical Optimization*, the
SQP restoration / elastic-mode discussion. Penalty and acceptance rules
below were fixed **before** coding, per the blueprint's DO-NOT-DO.

## 1. Trigger and scope

1. **Trigger.** Exactly one: `qp::solve_qp` reports `primal_infeasible`
   for the standard SQP subproblem (nlp-local-sqp §4.1). Every other QP
   outcome keeps the pre-existing paths (optimal → step; anything else →
   the hessian-reset fallback).
2. **Off switch.** `SqpOptions::elastic_restoration = false` restores
   the exact NLP-01 behavior: immediate inconclusive `NumericalFailure`
   with a message naming the disabled restoration. The paired benchmark
   (§8) and the regression test use it as the old path.
3. **What it is.** An elastic subproblem plus a violation-decrease
   acceptance gate — not a second nonlinear engine. Restoration steps
   make no objective-progress claim, no convergence claim and no
   multiplier claim (§4).
4. **What it is never.** It never converts a failure into a proof of
   infeasibility (blueprint: SQP failure is not a proof), never relaxes
   the original constraints (slacks exist only inside the elastic QP and
   are discarded), and never changes a status outside the existing
   nlp-local-sqp §5 vocabulary.

## 2. Frozen failure corpus (measured before implementation)

Both cases were measured on the post-NLP-01 tree (revision `5eb9dfb`)
with the production Python binding, before any NLP-02 code existed:

1. **`x2_minus_one` — the minimized feasible failure.** `n = 1`,
   `f(x) = (x − 1)²`, single equality `h(x) = x² − 1 = 0`, start `x0 = 0`.
   The model is feasible (`x = ±1`; the same model from `x0 = 0.1`
   converges to `x = 1` in 3 iterations). At `x0` the constraint
   gradient vanishes (`J = [2x] = [0]`) while `h = −1`, so the
   linearized equality reads `0 · d = 1`: QP `primal_infeasible` →
   measured immediate `NumericalFailure`, message "… linearization is
   primal-infeasible at the current iterate; elastic restoration is
   deferred (NLP-02)".
2. **`linear_pair` — truly inconclusive.** `x ≤ 0`, `x ≥ 1`, start
   `0.5`: linear constraints, so the linearization is infeasible at
   **every** iterate and the original violation `max(x, 1 − x) ≥ 0.5`
   everywhere. Measured the same immediate failure. After NLP-02 it
   must **remain** inconclusive (§5, §7).

## 3. Elastic subproblem (exact construction)

On the trigger, with `n_ineq` inequality and `n_eq` equality rows at the
current iterate (`J`, `cvals` as in nlp-local-sqp §4.1), build a QP over
columns `[d, s]`, `n_slack = n_ineq + 2·n_eq` slack columns:

```
minimize   ½ [d;0]ᵀ diag(B_k, 0) [d;0] + ∇f(x)ᵀ d + ρ · Σ s_k
subject to J_i d − s_i             ≤ −g_i      (i < n_ineq, s_i ≥ 0)
           J_j d + s⁺_j − s⁻_j     = −h_j      (j < n_eq, s⁺,s⁻ ≥ 0)
           lb − x ≤ d ≤ ub − x                  (bound rows, zero in slacks)
           s ≥ 0                                (appended last)
```

1. **Convexity and feasibility.** `P = diag(B_k, 0)` is positive
   semidefinite (`B_k` positive definite), so the elastic QP is convex;
   slacks are unbounded above, so the elastic QP is **always feasible** —
   a `primal_infeasible` elastic outcome is treated as a rejected
   attempt (§4), never as a model claim.
2. **Row layout.** Constraint rows keep indices `0 … m−1` (the slack
   nonnegativity rows are appended after the bound rows), so QP dual
   indexing of the original rows is unchanged — although this path does
   not read it (§4.3).
3. **Penalty ρ.** Exact-penalty scale per Nocedal–Wright's elastic mode:
   ρ must dominate the row multipliers so slacks are used only as
   needed. ρ starts at `max(μ, 10)` (`μ` = current merit penalty), is
   re-floored at `max(ρ, μ, 10)` on every attempt, and on a rejected
   attempt escalates `ρ ← min(10ρ, 10⁸)` (monotone, never reset).
4. **Solve.** Same `qp::solve_qp` options and tolerances as the standard
   subproblem (shared `sqp_qp_options` helper), including the shared
   deadline.

## 4. Acceptance rule (original constraints only)

Let `d` be the first `n` components of the elastic solution.

1. **Trust cap.** `‖d‖∞` is capped at `kTrustRadius = 0.5`, the same
   shared constant as the standard step (nlp-local-sqp §4.2).
2. **Backtracking trials.** `t` takes the values `1, ½, …, 2⁻¹⁰`;
   each trial is `x⁺ = Π_bounds(x + t·d)` with violation
   `v⁺ = constraint_violation(model, x⁺)` evaluated on the **original**
   nonlinear constraints and bounds — slacks never enter this test.
3. **Accept the first trial satisfying both:**
   - **A1 (nonzero step):** `‖t·d‖∞ ≥ 1e-10`. A zero step is never
     progress (zero-step edge case).
   - **A2 (documented decrease):** `v⁺ ≤ (1 − θ) · v` with
     `θ = 0.05`, where `v` is the violation at the current iterate.
     When `v = 0` the rule reads `v⁺ ≤ 0` (stay exactly feasible).
     The slack-penalized objective cannot justify infeasibility: only
     the original violation decides.
4. **On acceptance:** the iterate moves to `x⁺`;
   `SqpSolution::restoration_steps` increments; the consecutive-failure
   counter resets; the point feeds the same `best_feasible_x` bookkeeping
   as any iterate (nlp-local-sqp §5.4). The L-BFGS curvature is **not**
   updated (elastic pairs describe the slack-penalized objective, not
   `f`), the NLP multipliers are **not** overwritten (an elastic dual at
   a slack-positive row is ρ, not an NLP multiplier), and the convergence
   gate is **not** evaluated on this iteration — the next iteration
   resumes ordinary SQP at `x⁺`.
5. **On rejection:** `SqpSolution::restoration_failures` increments, ρ
   escalates (§3.3), the iterate does not move, and the next iteration
   retries from the same point.

## 5. Boundedness and honest outcomes

1. One restoration attempt per major iteration; `max_iterations` bounds
   the total work, and each attempt reuses the shared deadline polls.
2. **Consecutive-failure budget:** `SqpOptions::max_restoration_failures`
   (default 5) rejected attempts in a row without an accepted step in
   between; on exhaustion the solve returns inconclusive
   `NumericalFailure` with `restoration_exhausted = true`, a message
   naming elastic restoration and the counts, and engine
   `failure_site = sqp_restoration`.
3. **Truly infeasible models** (`linear_pair`) hit that budget: every
   attempt fails A2 because the original violation cannot decrease.
   The outcome is inconclusive forever — never `Infeasible`, never
   `Optimal`, never a claim about the NLP itself.
4. **Recovered models** resume normal SQP; any published
   `LocalStationary` still passes the independent verifier gate of
   nlp-local-sqp §5 unchanged. Limit stops stay `ResourceLimit` /
   `IterationLimit`.

## 6. Diagnostics and interfaces

1. `SqpOptions`: `elastic_restoration` (default `true`),
   `max_restoration_failures` (default 5). Existing options unchanged.
2. `SqpSolution`: `restoration_steps`, `restoration_failures`,
   `restoration_exhausted`. Every exit message carries
   "; elastic restoration steps=K" when K > 0, so the count reaches the
   JSON `message` as well.
3. Engine: `failure_site = sqp_restoration` and
   `suggested_recovery = improve_x0_or_relax_constraints` on exhausted
   exits; `sqp_solve` / `improve_x0_or_relax_kkt_tolerance` otherwise.
4. Python binding: `solve(..., elastic_restoration=bool)`; result keys
   `restoration_steps` and `restoration_failures`.

## 7. Test obligations

1. **Feasible-but-linearization-infeasible** (`x2_minus_one`, default
   options): the solve recovers — solver KKT status at a stationary
   point of the original model, `restoration_steps ≥ 1`, original
   violation below tolerance (`nlp_restoration_test`).
2. **Old path** (same case, `elastic_restoration = false`): immediate
   `NumericalFailure` naming the disabled restoration; zero restoration
   counters (`nlp_restoration_test`).
3. **Truly infeasible** (`linear_pair`, default options): honest
   inconclusive status, `restoration_exhausted`, bounded failure count,
   `best_feasible_x` empty, never `Infeasible`/`Optimal`
   (`nlp_restoration_test`; the existing §6.7 check in
   `nlp_local_semantics` re-runs unchanged as regression).
4. **Healthy-path regression:** standard local cases show
   `restoration_steps == restoration_failures == 0` and the full
   existing suites stay green (`nlp_restoration_test` + all NLP suites).
5. **Binding:** the recovery and off-switch outcomes through
   `markov_cero.NlpModel.solve` (`test_nlp02_bindings.py`).

## 8. Benchmark obligation

A paired record (own file under `evidence/`, indexed from
`evidence/INDEX.md`, never merged with other summaries): the frozen
NLP-01 §7 corpus plus `x2_minus_one`, each case run twice through the
public binding — restoration on and off. Per run: status, restoration
counts, iterations, callback evaluations, wall time. Summary: converged
counts on vs off (robust-convergence rate) and paired wall overhead on
the cases that succeed in both configurations. No cross-solver ranking,
no global claim, no speed claim beyond the paired in-process numbers.

## 9. Limits

1. **Not a feasibility proof.** Neither a recovered solve nor an
   exhausted budget says anything about global feasibility; the honest
   status rules of nlp-local-sqp §5 apply verbatim.
2. **Not a second nonlinear engine.** There is no inner feasibility
   phase, no nonlinear restoration subproblem, no acceptance based on
   the elastic objective. If the geometric decrease of A2 stalls (e.g.
   violation at a numerical floor, flat geometry), the bounded budget
   declares the run inconclusive — a documented failure mode, not a bug.
3. **Multipliers.** Elastic QP duals are never exported as NLP
   multipliers; multiplier estimates only come from standard-subproblem
   solves.
4. **Specification values.** `θ = 0.05`, step floor `1e-10`, backtracking
   depth `2⁻¹⁰`, trust radius `0.5`, ρ grid `×10 ≤ 10⁸` and the budget
   default `5` are contract values. Changing any of them is a contract
   change: bump this document, follow numerical-policy §8 for status
   implications, and re-run §7/§8.

## 10. Change procedure

Status or assurance-label effects follow
[numerical-policy](numerical-policy.md) §8 (none are introduced by v1:
the status vocabulary is unchanged). Rule, constant or budget changes
bump this contract's version with a migration note and re-run the §7
tests and the §8 paired record. Cross-references in
[nlp-local-sqp](nlp-local-sqp.md) §4.4/§8 are updated in the same
change.
