# Convex QP path contract (v1)

Binding contract for how `api::solve` qualifies, solves and discloses convex
quadratic programs. It closes blueprint task QP-01: the Hessian convention is
frozen, convexity classification is three-outcome, every accepted witness is
re-checked in original units, and the result discloses the actual CPU/GPU
path that ran.

Related contracts and evidence:

- [Numerical contract](numerical-policy.md) — `assurance` derivation for the
  witness types below; tolerance philosophy the verifier tolerances obey.
- [Resource limits contract](resource-limits.md) — stop attribution for the
  deadline/fill stops in §4.
- `evidence/INDEX.md` — benchmark records for the QPLIB obligation (§6).

Standard-form convention follows Stellato et al., "OSQP: an operator splitting
solver for quadratic programs" (2020).

## 1. Frozen convention: Hessian and canonical form

1. **Canonical form.** Every QP is converted to
   `minimize ½ xᵀP x + qᵀx + offset  subject to  l ≤ A x ≤ u`
   (`qp::QuadraticModel`, `include/markov_cero/qp/model.hpp`). The ½ factor
   is part of the convention: `SparseSymmetricMatrix::evaluate_energy`
   returns the full-symmetric quadratic form (off-diagonal contributions
   doubled) and every objective recomputation — solver, equilibration and
   verifier — uses `0.5 * evaluate_energy + qᵀx`.
2. **Storage.** `P` is upper-triangular CSC (`i ≤ j`, strictly increasing row
   indices per column); `A` is CSC. `P.multiply` expands to the full
   symmetric matrix. Any loader or test that violates upper-triangular
   storage fails `validate()`, it is never silently symmetrized.
3. **Sense.** `maximize` negates the internal objective inside the solver and
   flips the sign of the published `row_duals`/`reduced_costs` at the API
   boundary (`src/api/engine_qp.cpp`); the canonical form is always a
   minimization.
4. **Bounds are rows.** `qp::make_quadratic_model` folds every variable box
   bound into the unified `l ≤ A x ≤ u` row system, so the canonical dual
   vector `y` holds one multiplier per canonical row: the first
   `model.matrix.row_count` entries are the row duals and the trailing
   per-variable entries are reported as reduced costs. This split is part of
   the convention; a change to it changes this section in the same commit.

## 2. Convexity scope: three outcomes, never two

1. **Classifier.** `qp::assess_convexity` runs sparse LDLᵀ elimination with
   minimum-degree ordering (`src/qp/model_convexity.cpp`). The pivot
   threshold is `tolerance * matrix_scale` where `matrix_scale = max(1,
   max|Pij|)` and the default tolerance is `1e-10`.
2. **Three outcomes, frozen:**
   - `positive_semidefinite` — every pivot ≥ −pivot tolerance; solve proceeds.
   - `non_convex` — a pivot < −pivot tolerance certifies a negative
     direction; maps to `QpStatus::non_convex` → `InvalidModel`,
     `failure_site = qp_convexity_check`.
   - `indeterminate` — a near-zero pivot with an active Schur edge, fill
     budget exceeded, queue exhaustion, or an invalid/non-finite input. Maps
     to `QpStatus::unsupported` → `Unsupported`,
     `failure_site = qp_convexity_check_indeterminate`. **Indeterminate is
     never reported as PSD and never as non-convex.**
3. **Caps** (enforced, reported by their own name, never converted to a
   different status):

   | Cap | Value | On violation |
   |---|---|---|
   | convexity checker factor budget | 5,242,880 nonzeros (5·1024²) | `unsupported`, `qp_convexity_check_indeterminate` |
   | KKT factor `L` fill cap (`kMaxKktFactorNonzeros`) | 10,485,760 nonzeros (10·1024²) | `unsupported`, `qp_kkt_factor_fill_limit` |
   | deadline during convexity check or KKT factorization | caller deadline | `resource_limit`, `stop_reason = deadline_exceeded` |

4. **Tolerance separation.** Three scales exist and are never interchanged:
   PSD pivot tolerance (`1e-10 * matrix_scale`), ADMM residual tolerances
   (`absolute = relative = 1e-6` set by `src/api/engine_qp.cpp`), and the
   independent verifier acceptance tolerance (`1e-4` at the engine gate,
   witness verifiers `1e-6`; `verify_qp_solution` rejects any requested
   tolerance > 1e-4). The PSD pivot tolerance never gates a residual
   decision, and no residual tolerance ever substitutes for the PSD check —
   `verify_qp_solution` re-certifies PSD before accepting any witness.

## 3. KKT system, ADMM and independent verification

1. **System.** The ADMM linear system is the symmetric quasi-definite matrix
   `[P + σI Aᵀ; A −diag(ρ)⁻¹]` (`src/qp/kkt.hpp`), factorized by Davis's
   sparse LDLᵀ (Algorithm 849): SQD structure guarantees a nonsingular
   factorization without numerical pivot searching. Defaults: `σ = 1e-6`,
   `ρ_init = 0.1`, relaxation `α = 1.6`, adaptive ρ ∈ [1e-6, 1e6] every 25
   iterations (Boyd et al. 2011 §3.4.1).
2. **Conditioning.** `condition_estimate` is the pivot-ratio proxy
   `max|D| / min|D|` of the KKT LDLᵀ diagonal — a screening signal, never a
   true κ(K) and never a pass/fail gate.
3. **Convergence gate.** An iterate is `optimal` only when both ADMM
   residuals are within `absolute + relative * scale` **and** the local KKT
   gate (`src/qp/convergence.hpp`) passes: row feasibility against `[l, u]`,
   complementarity with `y` selecting the bound by its sign, and stationarity
   `P x + q + Aᵀy = 0`, each with the same `1e-6` tolerances.
4. **Independent verification in original units.** Before any status leaves
   the engine, the witness is re-checked against the *original* model — after
   equilibration has un-scaled it — by `qp::verify_qp_solution`
   (`src/qp/verifier.cpp`), in long-double accumulation: primal bound
   feasibility, stationarity, bound-side sign and complementarity of every
   multiplier, objective discrepancy under the ½ convention, integrality
   (MIQP incumbents), plus a fresh PSD certification of `P`. A rejected
   witness maps to `NumericalFailure` with
   `failure_site = qp_kkt_verification` and is **never** published as
   `Optimal`, whatever the solver reported.
5. **Infeasibility and unboundedness witnesses.** `QpStatus::primal_infeasible`
   is published as `Infeasible` only after `verify_qp_infeasibility` accepts
   the Farkas candidate (default tolerance 1e-6); `QpStatus::dual_infeasible`
   is published as `Unbounded` only after `verify_qp_unbounded` accepts the
   feasible anchor plus recession ray. A rejected witness is
   `NumericalFailure` with `failure_site = qp_primal_infeasibility_certificate`
   or `qp_dual_infeasibility_certificate`.
6. **Symbolic cache rule.** Repeated solves through one `AdmmQpSolver`
   session skip the symbolic pass only when the built KKT pattern matches the
   cache exactly (dimensions plus an exact copy of column pointers and row
   indices, fingerprinted). Numeric-only changes (ρ, σ) go through
   `update_numeric` and never invalidate the cache; a changed `P`/`A`
   pattern, a changed dimension or a disabled cache falls back to a full
   symbolic — a cache miss is bit-for-bit the uncached behavior. Counters
   `symbolic_factorizations`/`symbolic_reuses` expose every decision.

## 4. Status, certificate and stop mapping

`qp::QpStatus` maps to public statuses exactly as follows (the boundary
applies the resource contract on top; see `src/api/engine_qp.cpp`):

| `QpStatus` | status | `certificate_type` | `failure_site` |
|---|---|---|---|
| `optimal` (verifier passed) | `Optimal` | `convex_qp_kkt` | `none` |
| `optimal` (verifier rejected) | `NumericalFailure` | `none` | `qp_kkt_verification` |
| `primal_infeasible` (witness accepted) | `Infeasible` | `convex_qp_kkt` | `qp_primal_infeasibility_certificate` |
| `primal_infeasible` (witness rejected) | `NumericalFailure` | `none` | `qp_primal_infeasibility_certificate` |
| `dual_infeasible` (witness accepted) | `Unbounded` | `convex_qp_kkt` | `qp_dual_infeasibility_certificate` |
| `dual_infeasible` (witness rejected) | `NumericalFailure` | `none` | `qp_dual_infeasibility_certificate` |
| `non_convex` | `InvalidModel` | `none` | `qp_convexity_check` |
| `unsupported` (convexity/indeterminate) | `Unsupported` | `none` | `qp_convexity_check_indeterminate` |
| `unsupported` (KKT fill cap) | `Unsupported` | `none` | `qp_kkt_factor_fill_limit` |
| `iteration_limit` | `IterationLimit` | `none` | `qp_admm_iteration_limit` |
| `time_limit` | `resource_limit` + `stop_reason = deadline_exceeded` | `none` | `qp_wall_clock_deadline` |
| `numerical_error` | `NumericalFailure` | `none` | `qp_kkt_factorization` |

Notes:

- `certificate_type` is set **only when the corresponding witness passed**;
  a rejected witness publishes `none` (same conditional pattern as the LP
  engine's `canonical_lp_witness`). The single QP witness name
  `convex_qp_kkt` covers accepted optimality, infeasibility and
  unboundedness witnesses exactly as `canonical_lp_witness` covers LP —
  `assurance` derivation in [numerical-policy.md](numerical-policy.md) §2
  needs no new rows for it.
- `assurance` is derived once at the boundary from these fields; engines
  never set it. A verified QP witness on a global status yields
  `optimality_witness_checked`; a rejected one yields at most
  `original_primal_checked`, usually `unverified`.
- Deadline stops are attributed through the shared context
  (RES-01): the `time_limit` branch records `deadline_exceeded` even when
  the ADMM's own timer fired before the shared context saw one.

## 5. CPU/GPU path disclosure

Three distinct fields, all part of the public result:

| field | meaning |
|---|---|
| `backend` (CLI JSON) | the caller's request (`cpu`/`gpu`) |
| `recommended_backend` | auto-dispatch's threshold recommendation: `gpu` only when the caller asked **and** `quadratic_nonzeros > kQpGpuNnzThreshold` (100,000) |
| `backend_actually_used` (new in QP-01) | what actually executed: `cpu`, `cuda`, or `cpu_fallback` (GPU requested, path not taken) |

1. **QP sets the executed path from the solver's own telemetry.**
   `QpSolution::gpu_path_active` is true only when all three gates passed:
   explicit request, `NNZ(P) > 100,000`, and a CUDA device with a valid
   context. `api::solve` publishes `backend_actually_used = cuda` when it is
   active, `cpu_fallback` when GPU was requested but the gate failed, and
   `cpu` otherwise. PDLP publishes its equivalent from
   `PdlpResult::backend_actually_used`.
2. **The GPU path is partial by contract.** When active, only the per-iteration
   `P·x` residual product runs on the device; KKT factorization, all `A`
   operations, projections and verification remain on the CPU, with one
   host→device `x` upload and one `Px` download per iteration. There is no
   full-GPU QP solve in this version and none may be implied by any field.
3. **Fallback is silent in behavior, never silent in output.** An unmet GPU
   request must produce the identical CPU result (same status, objective and
   witnesses) and must say so via `backend_actually_used = cpu_fallback`;
   it never crashes and never claims `cuda`.

## 6. Test and benchmark obligations

1. **Attack suite** (`tests/qp_kkt_attack_test.cpp`): every rejected-witness
   path above must be exercised — wrong active side, altered/scaled/sign-
   flipped multipliers, complementarity breach, non-finite witness, and
   non-PSD `P` — and every case must be rejected by the independent
   verifier, never accepted as `Optimal`.
2. **Edge cases**: zero Hessian (QP degenerating to an LP), rank-deficient
   singular PSD `P` (flat optimal face), equality bounds (`l = u`), empty
   rows and unbounded boxes (`dual_infeasible` with an accepted recession
   ray) must solve or fail with the statuses of §4 — not with an
   `indeterminate` mislabel and not with an unverified `Optimal`.
3. **Repeated symbolic cache** under changed numeric data: ρ-only changes
   reuse the symbolic pass, a changed `P`/`A` pattern invalidates it, and
   two models with identical pattern but different `P` values both reuse the
   symbolic and produce independently verified witnesses.
4. **Benchmark**: the convex supported subset of `data/qp` (QPLIB-derived)
   is compared against OSQP (preregistered) and HiGHS where applicable,
   with this contract's original checks applied to every solution;
   statuses, objectives and fill/iteration behavior are recorded in
   `evidence/` and indexed from `evidence/INDEX.md`.
5. **Disclosure**: a test asserts that a GPU request surfaces the true
   `backend_actually_used` through `api::solve`/CLI JSON — `cuda` only when
   `gpu_path_active`, `cpu_fallback` otherwise.

Executed findings (2026-09-30, this checkout): obligations 1, 2, 3 and 5
are green in CTest `qp_kkt_attack` (99/99 suite). Obligation 4 was executed
through `scripts/bench_qp_kkt.cpp` and `scripts/run_qp_compare.py` over the
four tracked QPLIB instances: 4/4 markov solves are `Optimal` with accepted
`convex_qp_kkt` witnesses (objective discrepancy ≤ 1.6e-13), all 8
competitor solutions (OSQP 1.1.3, HiGHS 1.15.1) pass the primal check, and
all 8 objective comparisons agree with markov at the 1e-4 tolerance (worst
relative difference 2.1e-6). KKT `L` fill measured 5–57 nnz over 17–49 ADMM
iterations (two instances refactured ρ once), no fill-cap hit. Recorded in
[`evidence/qp-kkt-bench-20260930.json`](../../evidence/qp-kkt-bench-20260930.json)
and indexed from `evidence/INDEX.md`. The measured instances are the small
tracked subset, not the full QPLIB range — no breadth or speed claim.

## 7. Change procedure

Any change to this contract (a convention change in §1, a new convexity
outcome, a changed cap or tolerance, a changed status mapping or a changed
disclosure field) must update this page in the same change, keep the tests
named in §6 green, re-run `ctest -j8`, and — if `assurance` derivation or
certificate semantics move — update
[numerical-policy.md](numerical-policy.md) under its own change procedure.
The reported strings in §4 and §5 (`certificate_type`, `failure_site`,
`backend_actually_used` values) are part of the contract; they may not be
renamed without a version bump here.
