# Numerical and status contract — v1

**Contract version:** 1 (schema: `assurance` field introduced 2026-09-29).
**Status:** binding for every change under blueprint task NUM-01 and its dependents (LP-01, QP-01, MIP-01, MIQP-01, NLP-01, MINLP-01).
**Scope:** every tolerance, status, verification flag and assurance label emitted by `api::solve_file` / `api::solve_model`, the CLI JSON and the Python binding.

Rules this contract exists to enforce (blueprint §5 and §9):

1. A **solver convergence knob** and a **verifier acceptance threshold** are different quantities with different names. A pivot tolerance is never a feasibility tolerance.
2. Solver status, primal feasibility and global assurance are **separate fields**.
3. Verifiers recompute from model data; they never trust solver telemetry.
4. Every accepted global status names a **passing witness checker**. No checker, no global claim.
5. No tolerance changes without an error analysis. Loosening a check to preserve a status is forbidden.

---

## 1. Two verification boundaries

| Boundary | Data used | What it can establish | Cannot establish |
|---|---|---|---|
| **Original-model checker** (`verify_primal`, `verify_nlp_feasibility`) | the `model::Model` handed to the API, in user units | row/variable bounds, integrality, objective value, callback feasibility | optimality, globality |
| **Class-specific checker** (`verify_sparse_result`, `verify_qp_solution`, `verify_nlp_solution`, `verify_mip_proof`) | the canonical/working model of that class | LP dual/ray/Farkas witness, convex QP KKT, NLP first-order KKT, MILP/MIQP branch tree | anything outside its class assumption |

A verifier recomputes activities, residuals and objectives from input and compares against **its own** declared threshold. It never reads a residual the engine computed about itself.

`verified`, `canonical_verified` and `original_verified` are independent booleans:

- `original_verified` — original-model checker accepted the returned primal (or is not applicable, e.g. infeasible/unbounded results carry no primal).
- `canonical_verified` — class-specific checker accepted the witness for the claimed status.
- `verified` — computed once in `api::detail::finalize` (`src/api/api.cpp:130`): `optimal` requires both checkers; `infeasible`/`unbounded` require the class-specific checker. A recorded resource stop forces `verified = false` (`src/api/api.cpp:110`).

---

## 2. Assurance labels (new in contract v1)

`SolveResult::assurance` (string, default `"unverified"`) is the **typed, single-word summary of the strongest check that actually passed**. It is derived in one place (`api::detail::finalize`) from status, flags, `certificate_type` and `guarantee_tier`; engines never set it directly. It is additive: no existing field changes meaning.

| Label | Meaning | Emitted when |
|---|---|---|
| `tree_replayed` | an independent, bounded, cut-free MILP/MIQP proof tree was rebuilt and replayed | `proof_status == "accepted"` and `guarantee_tier ∈ {independent_tree, replayed_tree}` |
| `optimality_witness_checked` | a class-specific optimality/infeasibility/unboundedness witness (LP dual gap, Farkas ray, recession ray, convex QP KKT) passed, **and** no un-replayed global claim is being made | `status ∈ {Optimal, Infeasible, Unbounded, GapSatisfied}`, `canonical_verified`, `certificate_type` is a witness type, and the certificate is not solver-trusted |
| `local_kkt_checked` | a first-order NLP KKT candidate passed the independent NLP checker | `certificate_type == "local_kkt"` |
| `original_primal_checked` | only the original-model primal/integrality/objective check passed | `original_verified` and none of the above apply (MILP incumbents without an accepted proof, solver-trusted MINLP OA results, PDLP primal-feasibility-only results) |
| `unverified` | no checker accepted, or the result was stopped/failed/limited | everything else |

**Non-global labels.** `local_kkt_checked` and any MINLP result are **not global certificates**:

- `local_kkt_checked` is a first-order necessary condition. It does not prove a local minimum (a saddle satisfies it) and never proves globality. `local_minimum_verified` is reserved for a future second-order sufficiency test and is not emitted today.
- MINLP with `certificate_type == "incumbent_feasibility; solver_trusted_oa"` publishes `original_primal_checked`. The OA master bound is solver-trusted until MINLP-02 adds an independent OA replay; only then may `oa_replayed` be emitted. A small OA gap never justifies `optimality_witness_checked`.
- `guarantee_tier == "unverified"` always accompanies `assurance ∈ {original_primal_checked, unverified}` for MILP results.

---

## 3. Status enum and required witness

`lp::reference::SolveStatus` (`include/markov_cero/lp/reference/revised_simplex.hpp:12`), JSON spelling from `to_string` (`src/lp/reference/revised_simplex.cpp:200`), exit code from `apps/json_output.hpp:19`.

| JSON status | Meaning | Required passing witness for the claim | `assurance` ceiling |
|---|---|---|---|
| `Optimal` | global optimum of the stated class | LP: canonical primal+dual gap; QP: original-unit KKT; MILP/MIQP: replayed proof tree, else downgraded to `Feasible` | `optimality_witness_checked` / `tree_replayed` |
| `Infeasible` | no feasible point exists | canonical Farkas/alternative witness (LP, QP) | `optimality_witness_checked` |
| `Unbounded` | objective unbounded over a feasible set | feasible anchor + improving recession ray | `optimality_witness_checked` |
| `GapSatisfied` | bound within the declared gap tolerance; **not** exact optimality | best valid bound + incumbent, both verified | `optimality_witness_checked` (never `tree_replayed` unless a proof was accepted) |
| `Feasible` | verified incumbent only; global conclusion unverified | original-model primal + integrality | `original_primal_checked` |
| `LocalOptimal` | first-order local KKT candidate for NLP | independent NLP KKT checker | `local_kkt_checked` |
| `ResourceLimit` | stop reason hit (deadline/memory/nodes/queue) | none; incumbent/bound retained only if separately verified | `original_primal_checked` or `unverified` |
| `IterationLimit` | iteration budget hit | none | as verified, else `unverified` |
| `NumericalFailure` | a required witness was rejected | none | `unverified` |
| `InvalidModel` / `InvalidOptions` / `Unsupported` / `NonConvexMINLP` | rejected before/at solve | none | `unverified` |

Hard invariants (blueprint §5, enforced by tests):

- A resource stop never accompanies an unverified `Optimal`/`Infeasible`/`Unbounded` (`src/api/api.cpp:110`).
- Proof-budget exhaustion never converts to `Optimal`; the incumbent stays original-verified and the status becomes `Feasible` (`src/api/engine_milp.cpp:77-84`).
- `gap_satisfied` and `optimal` remain distinct statuses.
- An indeterminate PSD result is `Unsupported`/`NonConvex`, never `Optimal`.

---

## 4. Residual formulas

These are the definitions every verifier implements. `abs` and `rel` are the per-quantity thresholds in §5.

**Row violation (original model, active side only):**

```text
v_i = max(row_lower_i - (Ax)_i, (Ax)_i - row_upper_i, 0)
allowed_i = abs + rel * max(1, |row bound_i|, Σ_j |A_ij x_j|)
accept iff v_i <= allowed_i for every row i
```

Rows with infinite bound on the active side contribute no violation on that side. A single huge RHS row therefore cannot hide a small-row violation: the test is per row, never a global norm.

**Variable bound violation:** same form per variable, using `|bound_j|` and `|x_j|`.

**Integrality:** `|x_j - round(x_j)| <= integrality_tolerance` for `x_j` declared integer only, plus binary bounds `0 <= x_j <= 1`. Continuous variables are never rounded or checked.

**Objective:** recomputed from the original model with wider accumulation (long double where implemented) and compared with `|reported - recomputed| <= abs + rel * max(1, |recomputed|)`. Objective **sense** is applied before comparison; maximization results are compared in user-facing sign.

**LP optimality gap:** `|cᵀx - bᵀy| <= abs + rel * max(1, |cᵀx|, |bᵀy|)` together with primal feasibility `x_B >= -tol` and dual feasibility `r_N >= -tol`.

**Relative MIP gap:** `(U - L) / max(1, |U|)` where `U` is a verified incumbent objective and `L` a verified bound, each guarded in its own direction (incumbent guarded upward, bound guarded downward).

**QP KKT (original units):** primal feasibility, stationarity `Px + q + Aᵀy + bound multipliers = 0`, dual sign on the active side of each row/bound, and complementarity `|(Ax-u)ᵀy⁺| + |(l-Ax)ᵀy⁻| + bound terms <= allowed`. The PSD check runs **before** any optimality claim.

**NLP first-order KKT:** stationarity with the bound-normal decomposition, inequality multipliers `>= 0`, complementarity, and primal feasibility of `g`, `h`, bounds — all recomputed from the user callbacks at the accepted point.

---

## 5. Tolerance inventory

Role: **S** = solver convergence knob, **V** = verifier acceptance threshold, **S/V** = used as both (explicitly listed), **guard** = input validation, **dead** = declared but unused.

### 5.1 Reference (primal) simplex

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `feasibility_tolerance` | `revised_simplex.hpp:30` | 1e-9 | primal feasibility of basic variables; phase-I objective gate | S |
| `dual_tolerance` | `revised_simplex.hpp:31` | 1e-9 | reduced-cost pricing admissibility | S |
| `pivot_tolerance` | `revised_simplex.hpp:32` | 1e-12 | ratio-test pivot denominator; basis crash threshold | S |
| `maximum_tolerance` | `revised_simplex_internal.hpp:34` | 1e-4 | option validation cap; also requires `pivot <= feasibility` | guard |
| snap floor | `revised_simplex_pricing.cpp:101` | `max(feas, 1e-6, 1e-8·max_norm)` | snap near-bound basic values | S |
| refactor gate | `revised_simplex_iteration.cpp:32` | 1e-5 | refactorize when a basic value breaches −1e-5 | S |
| `certify` re-check | `revised_simplex.cpp:102,129,145` | `max(feas,dual)` = 1e-9 | simplex re-verifies its own witness before returning | V |

### 5.2 Dual simplex

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `feasibility_tolerance` | `dual_simplex.hpp:30` | 1e-9 | primal feasibility of basic variables | S |
| `dual_tolerance` | `dual_simplex.hpp:31` | 1e-9 | reduced-cost guard and pricing ratio numerator | S |
| `pivot_tolerance` | `dual_simplex.hpp:32` | 1e-12 | ratio test `alpha >= -pivot` | S |
| `condition_trigger` | `dual_simplex.hpp:33` | 1e-14 | refactorize on condition proxy | S |
| `maximum_tolerance` | `dual_simplex_internal.hpp:36` | 1e-4 | option validation cap | guard |
| internal witness gate | `dual_simplex_select_entering_column.cpp:67,88,195` | 1e-9 | full reference re-check before accepting basis/feasibility/ray | V |

### 5.3 Interior point

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `relative_tolerance` | `ipm.hpp:30` | 1e-8 | scaled residual/gap targets: `tol·(1+|b|)`, `tol·(1+|c|)`, `tol·(1+|cᵀx|)` | S |
| normal-matrix delta | `ipm_solve.cpp:69,109` | 1e-12 | normal-equation regularization | S |
| LU `singular_tolerance` | `ipm_solve.cpp:114` | 1e-14 | factorization failure threshold | S |
| perturbation ladder | `ipm_solve.cpp:116` | 1e-10 … 1e-2 | infeasibility recovery | S |
| iterate clamp | `ipm_solve.cpp:106` | [1e-12, 1e12] | primal/dual iterate bounds | S |
| step floor | `ipm.cpp:152` | 1e-14 | minimum step and refinement exit | S |
| crossover ξ / pivot | `ipm_crossover_basis_sparse.cpp:9,50` | 1e-7 / 1e-10·max(1,‖·‖) | basis extraction | S |

### 5.4 PDLP

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `primal_tolerance`, `dual_tolerance`, `gap_tolerance` | `pdlp.hpp:23-25` | 1e-4 each | **relative** residual convergence targets | S |
| `SolveOptions::pdlp_tolerance` | `solve.hpp:37` | 1e-4 | CLI/API override; also the PDLP **verifier** threshold for `verify_linear_solution` and `verify_primal` | S/V |
| `stagnation_threshold` | `pdlp.hpp:41` | 0.999 | relative-improvement window | S |
| crossover dual-simplex tol | `pdlp_try_dual_simplex_crossover.cpp:59` | 1e-7 | crossover basis solve | S |
| basis ξ / pivot | `pdlp.cpp:168,193` | 1e-7 / 1e-10·max(1,‖·‖) | basis extraction | S |
| `crossover_primal_tolerance`, `crossover_dual_tolerance` | `pdlp.hpp:42-43` | 1e-4 | **dead** — declared, never read | dead |

### 5.5 Convex QP / ADMM

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `absolute_tolerance`, `relative_tolerance` | `admm_solver.hpp:28-29` | 1e-4 (API overrides to 1e-6 in `engine_qp.cpp:12-13`) | ADMM primal/dual residual convergence | S |
| `primal_infeasible_tolerance`, `dual_infeasible_tolerance` | `admm_solver.hpp:30-31` | 1e-5 | Farkas / recession detection | S |
| `verify_qp_solution` tolerance | `qp/verifier.hpp:26` (capped `(0,1e-4]`) | 1e-4 (node QP passes 1e-7) | KKT witness acceptance | V |
| `verify_qp_infeasibility` / `_unbounded` | `qp/verifier.hpp:28-29` | 1e-6 | Farkas / recession acceptance | V |
| PSD floor | `qp/model.hpp:92` via `model_convexity.cpp:39` | 1e-10 (`tol·scale`) | sparse-LDL pivot floor for PSD classification | S+V |
| KKT LDL pivot floor | `kkt_factor.cpp:124` | 1e-15 | factorization failure | S |
| `sigma`, `rho_init`, `alpha` | `admm_solver.hpp:32-34` | 1e-6 / 0.1 / 1.6 | regularization, penalty, over-relaxation | S |
| SQP subproblem tolerance | `sqp_solver.cpp:78` | 1e-8 | convex QP subproblem inside SQP | S |

### 5.6 NLP / SQP

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `kkt_tolerance` | `sqp_solver.hpp:34` | 1e-6 | SQP convergence gate | S |
| `armijo_constant`, `wolfe_curvature` | `sqp_solver.hpp:35-36` | 1e-4, 0.9 | line-search sufficient decrease / curvature | S |
| `kActiveTolerance` | `sqp_solver_internal.hpp:20` | 1e-10 | active-set classification in stationarity | S |
| bound-active / fixed | `sqp_solver_subproblem.cpp:12-16` | 1e-9 / 1e-12 | bound treatment in the step QP | S |
| L-BFGS guards | `lbfgs.cpp:18,90,106` | 1e-12 / 1e-18 | curvature and denominator guards | S |
| `verify_nlp_solution` / `verify_nlp_feasibility` | `nlp_verifier.hpp:39,43` | 1e-6 | independent KKT / callback feasibility acceptance | V |

### 5.7 MINLP (outer approximation)

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `gap_tolerance` | `minlp_solver.hpp:38` | **1e-3** | OA gap closure. Deliberately 10× looser than MILP's 1e-4 because the bound chain includes SQP subproblem accuracy (`minlp_solver.hpp:28-29`) | S |
| `feasibility_tolerance` | `minlp_solver.hpp:39` | 1e-6 | OA subproblem gate **and** the independent incumbent re-check in `engine_nonlinear.cpp:101-115` | S/V |

### 5.8 MILP search

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `relative_gap_tolerance` | `milp_solver.hpp:27` | 1e-4 | gap-closure test for `GapSatisfied` | S |
| `absolute_gap_tolerance` | `milp_solver.hpp:28` | 1e-6 | bound pruning `L >= U - abs` | S |
| `integrality_tolerance` | `milp_solver.hpp:29` | 1e-6 | fractionality and branching decisions | S |
| `feasibility_tolerance` | `milp_solver.hpp:30` | 1e-7 | node solution feasibility in heuristics | S |
| node LP certify slack | `node_lp.cpp:133` | `min(1e-4, max(feas,1e-8))` = 1e-7 | node relaxation witness acceptance | V |
| node QP verify | `node_qp.cpp:48` | `min(1e-4, max(1e-7, feas))` = 1e-7 | node QP witness acceptance | V |
| cut singular tolerance | `mir.cpp:32`, `gomory.cpp:23` | 1e-11 | cut-generation factorization | S |
| cut hygiene floors | `cut_pool.cpp:32,55,80` | 1e-24 / 1e-20 | efficacy and coefficient cleanup | S |
| `heuristics.hpp` / `strong_branching.hpp` defaults | as declared | 1e-6 / 1e-7 | used when a call site omits explicit tolerances | S |

`parallel_tree_search.hpp:25-28` duplicates the four `milp::Options` tolerances verbatim (second source of truth, same defaults) — see §7.

### 5.9 Presolve and scaling

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `feasibility_tolerance` | `presolve.hpp:16` | 1e-9 | reduction legality | S |
| `dual_tolerance` | `presolve.hpp:17` | 1e-9 | dual-improving reduction test | S |
| `pivot_tolerance` | `presolve.hpp:18` | 1e-12 | near-zero coefficient drop (name overlaps §5.1 — see §7) | S |
| `postsolve` tolerance | `presolve.hpp:51` | 1e-8 | **dead** — parameter ignored (`postsolve.cpp:7`) | dead |
| `RuizOptions::tolerance` | `ruiz_scaling.hpp:16` | 1e-3 | equilibration convergence | S |
| `RuizOptions::min_scale` | `ruiz_scaling.hpp:17` | 1e-4 | scale clamp floor | S |

### 5.10 Sparse/dense linear algebra

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `singular_tolerance` | `sparse_basis.hpp:73`, `dense_lu.hpp:21` | 1e-14 | factorization failure | S |
| `update_pivot_tolerance` | `sparse_basis.hpp:74` | 1e-12 | rank-1 update pivot | S |
| `early_exit_tol` | `sparse_basis.hpp:56` | 1e-14 | iterative-refinement early exit | S |
| `eta_density_trigger` | `sparse_basis.hpp:75` | 0.5 (both simplexes override to 0.9) | eta-matrix density trigger | S |

### 5.11 Independent verifiers

| Name | Location | Default | Meaning | Role |
|---|---|---|---|---|
| `verify::Tolerance{abs, rel}` | `primal_verifier.hpp:13` | 1e-7, 1e-7 (capped at 1e-4) | original-model row/bound allowance | V |
| integrality (original) | `primal_verifier.hpp:46` | 1e-6 (validated `< 0.5`) | original-model integrality acceptance | V |
| LP original-model integrality override | `engine_lp.cpp:277` | 1e-7 | LP path passes 1e-7 (tighter than the 1e-6 default) | V |
| `verify_sparse_result` / `verify_reference_result` | `reference_lp_verifier.hpp:16,20` (cap `(0,1e-4]`) | 1e-8 | canonical LP witness; API passes `max(feas,dual,1e-8)` | V |
| `roundoff_factor` | `sparse_lp_verifier.cpp:11` | 512·machine ε | added to every allowance | V |
| `verify_linear_solution` | `linear_certificate.hpp:17` (cap `(0,1e-4]`) | 1e-7 (PDLP path passes `pdlp_tolerance`) | original primal + weak-dual bound | V |
| `MipProofOptions::tolerance` | `mip_proof.hpp:59` (cap `(0,1e-4]`) | 1e-8 | proof replay gate | V |
| `MipProofOptions::relative_gap` | `mip_proof.hpp:60` | 0 (set to 1e-4 only for `gap_satisfied`) | gap allowance in bound closure | V |
| proof leaf solve headroom | `mip_proof_relaxation.cpp:55` | `tolerance · 0.1` | leaf relaxations are solved tighter than they are checked | S/V |

**Direction convention.** Verifiers are looser than the solver that produced the witness (solver 1e-9, canonical verifier 1e-8, original verifier 1e-7), because round-trip reconstruction and unscaling add error. The MIP proof is the one deliberate exception: leaves are solved at `tol·0.1` and checked at `tol`, giving the checker headroom rather than the solver. Both directions are intentional and are boundary-tested in `tests/numerical_policy_boundary_test.cpp`.

---

## 6. Boundary tests

`tests/numerical_policy_boundary_test.cpp` and `tests/assurance_label_test.cpp` construct witnesses **just inside** and **just outside** each acceptance threshold and assert accept/reject. A tolerance change that moves these boundaries must fail the test and require an error analysis.

Covered: original-model row violation; objective mismatch; integrality; canonical LP primal and the duality gap that binds with it; convex QP stationarity and the PSD gate; QP curvature classification (PSD / non-convex / indeterminate); NLP feasibility, KKT stationarity and dual sign; assurance-label derivation for each of the six labels, including the fail-closed cases (unreplayed proof tree, unchecked KKT candidate, solver-trusted OA, resource stop).

---

## 7. Documented divergences and open items

These are recorded, not silently accepted. None is a licence to change behaviour without the analysis noted.

| # | Observation | Status |
|---|---|---|
| D-1 | `pivot_tolerance` names four different quantities: simplex ratio test, presolve coefficient zeroing, linalg update pivot, and the QP PSD floor scale (`model_convexity.cpp:39`) | Documented; rename deferred (would touch many files, no behaviour change) |
| D-2 | Unnamed literals looser than their own solver tolerance: refactor gate 1e-5 and snap floor 1e-6 vs `feasibility_tolerance` 1e-9 | Documented; both are internal stability guards, not acceptance thresholds |
| D-3 | `presolve::postsolve` declares a `tolerance` parameter that is ignored | **Open** — dead parameter; remove or honour it in a presolve-focused change |
| D-4 | `pdlp.hpp:42-43` crossover tolerances are declared and never read | **Open** — dead declarations; remove in a PDLP-focused change |
| D-5 | QP `original_verified` and `canonical_verified` are assigned from one check (`engine_qp.cpp:53,63`), as are the NLP pair (`engine_nonlinear.cpp:54-55`) | Documented: for QP and NLP the working model *is* the original model (no presolve/scale stage runs on those paths), so both flags describe the same recomputation. The conjunction adds no independent assurance there; LP/PDLP remain the two-boundary pattern. Do not read QP/NLP flags as two independent checks. |
| D-6 | MINLP gap 1e-3 vs MILP gap 1e-4 | Intentional and documented at `minlp_solver.hpp:28-29` |
| D-7 | QP verifier's PSD gate is hard-wired to 1e-10 (`qp/verifier.cpp:27`) regardless of the caller's `tolerance` argument | Documented; PSD classification is a separate question from KKT residual tolerance |
| D-8 | `parallel_tree_search.hpp` duplicates `milp::Options` tolerances | **Open** — second source of truth; consolidate when the MILP options are next touched |
| D-9 | Heuristic/strong-branching header defaults (1e-6/1e-7) differ from `milp::Options` (1e-7) when call sites omit arguments | Documented; callers that matter pass explicit tolerances |

---

## 8. Change procedure

1. State the quantity, units, formula, old default, new default and the error analysis that justifies the change.
2. Update this file and the boundary test in the same change.
3. Re-run the full CTest suite; a status change without a stated mathematical reason is a stop condition (blueprint §29.2).
4. Bump the contract version if a status, assurance label or witness requirement changes.

---

## 9. Compatibility and migration note (contract v1)

Contract v1 adds a field and changes no existing one.

| Surface | Before v1 | With v1 | Action for a caller |
|---|---|---|---|
| `api::SolveResult` (C++) | no `assurance` member | `std::string assurance`, default `"unverified"` | read `assurance`; nothing to migrate |
| CLI JSON | `certificate_type`, `guarantee_tier`, `verified`, … | one added key: `"assurance"` | consumers that whitelist result keys must add `"assurance"` |
| Python binding dict | same set as CLI | one added key: `"assurance"` | same |
| Status enum, all prior keys | — | unchanged names, meanings and defaults | none |

Migration guidance:

- **Stop inferring rigor from `guarantee_tier` or `certificate_type`.** Both are inputs to the label, not the label: a solver-trusted MINLP bound may report a small gap and `verified = true` while `assurance` is `original_primal_checked`. The label is the single field that answers "what passed".
- **`assurance` is additive and monotone within one result.** It is computed once in `api::detail::finalize` after `apply_resource_stop`, so a resource stop can only lower the label. Engines and callers must never write it.
- **A default of `"unverified"` is honest, not missing.** Any `SolveResult` that never passed through `finalize` (default-constructed, aggregate-filled, or a stopped run) reports `unverified`.
- **Known deferred consumer:** `web/backend/server.py` whitelists `iterations`/`optimality_gap` and does not yet pass `assurance` (also still misses `lp_iterations`/`relative_gap`). Assigned to blueprint task REL-01; it is a reporting gap, not a solver-status change.
- **Versioning.** The schema is tracked by the contract version above. Renaming `assurance`, removing a label, or changing the meaning of an existing key requires contract v2 and a migration note here.
