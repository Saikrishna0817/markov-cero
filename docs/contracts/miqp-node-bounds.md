# MIQP node-bounds contract

**Contract version:** 1.
**Scope:** every lower bound, prune, incumbent and proof claim made by the
convex MIQP path — `src/milp/node_qp.cpp`, `src/qp/supporting_bound.cpp`,
the QP branch of `src/verify/mip_proof_relaxation.cpp`, and the MIQP
incumbent/proof gates in `src/api/engine_milp.cpp` and
`src/verify/mip_proof.cpp`.
**Authority chain:** this contract derives from blueprint §13
(`docs/project/INDUSTRY_GRADE_IMPLEMENTATION_BLUEPRINT.md`) and sits under
[convex-qp.md](convex-qp.md) (QP class, KKT witness),
[milp-node-bounds.md](milp-node-bounds.md) (search statuses, prune guards
F1–F3, partition rule, proof budgets), and
[numerical-policy.md](numerical-policy.md) (tolerances, assurance labels).
Nothing here loosens a rule in those contracts.

## 1. Supported class

1. **Source class.** A model with `has_quadratic_objective`, a symmetric
   `P ⪰ 0` under the frozen convention of
   [convex-qp.md](convex-qp.md) (½xᵀPx, upper-tri CSC, full-symmetric
   energy), mixed continuous/integer variables, linear rows and finite or
   infinite variable bounds. Non-convex, indeterminate-PSD and non-MIQP
   sources never reach a global claim (§3.4).
2. **Relaxation.** At node domain `D` the relaxation drops integrality and
   keeps all original linear rows plus the node's bound overlay
   (`node_qp.cpp:22-28` writes the overlay into `q.l/q.u` at rows `m+j`).
   QP nodes carry no basis: cuts, warm starts and strong branching are
   basis-gated off for MIQP and are inert, not silently divergent.
3. **Not claimed.** No general nonconvex MIQP, no global claim for an
   inconclusive node, no bound from a solver-reported primal objective
   alone (§2.5).

## 2. Supporting lower bound (`qp::supporting_lower_bound`)

### 2.1 Input space invariant

The function is defined only on a **minimization-normalized** model.
Whole-model normalization (negating `objective`, `objective_offset`,
`quadratic_matrix.value`) happens once at the search boundary
(`search_initialize.cpp:36-52`) and is negated back once at exit
(`:45-47`); proof replay normalizes the same way
(`mip_proof_internal.hpp:44-54`) and converts the recorded incumbent back
at `mip_proof.cpp:65`. Calling it on a raw maximize model — where a
sign-flipped `q`/`P` meets an un-negated `objective_offset` — produces
mixed-space output and is forbidden. Objective sense and offset therefore
move only through these boundaries, never inside the bound
(`mip_proof.cpp:113`, `engine_milp.cpp:24` restore the sign once).

### 2.2 Derivation

For `f(z) = ½zᵀPz + qᵀz + offset`, a recorded KKT point `(x̂, ŷ)` with
`P ⪰ 0`, the affine support and Lagrangian construction give, with
`px = P·x̂` (`supporting_bound.cpp:16`):

```text
c   = offset + Σⱼ (−½ · x̂ⱼ · pxⱼ)                  // :21, :24-25
sideᵢ = uᵢ if ŷᵢ > 0 else lᵢ,  termᵢ = −ŷᵢ · sideᵢ   // :30-33 (ŷᵢ = 0 → no term)
rⱼ  = qⱼ + pxⱼ + Σᵢ Aᵢⱼ · ŷᵢ                        // :23, :36-37
ξⱼ  = loⱼ if rⱼ ≥ 0 else hiⱼ,  termⱼ = rⱼ · ξⱼ        // :38-41 (rⱼ = 0 → no term)
L   = c + Σ_{i:ŷᵢ≠0} termᵢ + Σ_{j:rⱼ≠0} termⱼ − 512·ε·(1 + magnitude)   // :32, :41, :45
```

`magnitude` accumulates `|offset|`, each `|−½x̂ⱼpxⱼ|`, each `|−ŷᵢsideᵢ|`
and each `|rⱼξⱼ|` as they are added (`:21-45`).

1. **Row-multiplier sign (OSQP convention).** `ŷᵢ > 0` selects the upper
   endpoint `uᵢ`, `ŷᵢ < 0` selects the lower endpoint `lᵢ` — the same
   convention the KKT verifier enforces (`src/qp/verifier.cpp:58-66`).
   A zero multiplier contributes nothing and does **not** require the
   unused side to be finite (`:29`).
2. **Endpoint choice.** The box infimum of `rⱼzⱼ` over `[loⱼ, hiⱼ]` is
   attained at `loⱼ` when `rⱼ ≥ 0` and at `hiⱼ` when `rⱼ < 0`; `rⱼ = 0`
   contributes nothing and does not require either endpoint to be finite
   (`:38-40`). Accumulation of `rⱼ` (`:36-37`) precedes the endpoint
   choice (`:38`).
3. **Soundness.** Given `P ⪰ 0` and finite selected endpoints,
   `L ≤ min{f(z) : z ∈ D, original rows hold}` for **any** `(x̂, ŷ)`:
   convexity gives `f(z) ≥ f(x̂) + ∇f(x̂)ᵀ(z − x̂)`, and
   `ŷᵢ(aᵢᵀz − sideᵢ) ≤ 0` for feasible `z`. The KKT gate (§3.2) is a
   quality and hygiene gate; **`P ⪰ 0`, finite selected endpoints and
   the input-space invariant are the soundness preconditions.**

### 2.3 Fail-closed cases (always −∞, never a fabricated number)

- dimension or short-`y` mismatch (`:18-19`);
- a used side (`ŷᵢ ≠ 0`) is infinite (`:31`);
- a used endpoint (`rⱼ ≠ 0`) is infinite (`:40`) — the affine box
  infimum is genuinely −∞, so no finite bound exists;
- the accumulated value is not finite (`:46`, NaN included).

−∞ flows into `NodeLpResult::lower_bound`, which the node step maps to
`unsupported` (§3.3) — `max(parent, −∞)` is never reached, the inherited
bound survives, the node counts unresolved, and `proven` stays blocked
(milp-node-bounds §2 P5, `search_node_relaxation.cpp:60-85`).

### 2.4 Stored guard

The returned value is already weakened by `512·ε·(1 + magnitude)`
(`:45`), a directed downward guard covering long-double-to-double
rounding and summation error across the final products. This guard is
**stored in the value**; the separate prune-time guard
`milp::prune_guard` (1e-10, milp-node-bounds §3.1) is applied at the
prune comparisons and is never subtracted from the stored bound. Both
guards push the reported bound down, never up.

### 2.5 Provenance

A bound used for pruning is `supporting_lower_bound` evaluated from the
KKT-verified witness (`node_qp.cpp:54-55`) — **never**
`sol.objective_value` (`:63`, which serves only pseudo-costs and
near-integral comparisons). Prune sites are exactly the shared ones:
inherited-bound prunes (`milp_solver.cpp:43-46`,
`parallel_tree_search.cpp:44-49`, `work_queue.cpp:115,159`), the
post-relaxation finite+guard prune (`search_node_relaxation.cpp:92-96`,
`parallel_tree_search.cpp:81-86`), and the F2/F3 aggregation
(`search_node_relaxation.cpp:88`, `search_finish.cpp:25-28,49-50`).

## 3. Node QP dispatch and status mapping

1. **Dispatch.** `solve_node_relaxation` routes
   `has_quadratic_objective` models to `solve_node_qp`
   (`node_lp.cpp:95-96`), including the serial root, node, re-solve and
   parallel entry points; warm starts are dropped.
2. **Witness gate.** The supporting bound is computed only after
   `verify_qp_solution` accepts the KKT witness at
   `tol = min(1e-4, max(1e-7, feasibility_tolerance))`
   (`node_qp.cpp:48,54-55`); a rejected witness never yields a bound.
3. **Status → P-table** (search reasons live in milp-node-bounds §2):

   | `NodeLpResult` outcome | search handling | reason |
   |---|---|---|
   | `optimal` + finite bound | F2 merge, then guarded prune | P1 |
   | `primal_infeasible` + verified Farkas | prune as infeasible (`search_node_relaxation.cpp:55-58`) | P2 |
   | `resource_limit` (QP deadline, `node_qp.cpp:38-42`) | retry once cold, then unresolved + bound folded | P5 / P8 |
   | `numerical_failure` (KKT reject, non-convex, PSD-indeterminate, exception) | same retry-then-unresolved path | P5 |
   | `unsupported` (non-finite supporting bound, `:56-60`) | same; inherited bound preserved | P5 |

   A root failure surfaces through `search_root_relaxation.cpp:20-31`
   with a finite `best_bound` set only when one exists (`:22-23`).
4. **Convexity.** PSD of `P` is re-assessed before any bound: the node
   path relies on the ADMM/KKT gate, and proof replay re-checks with
   `assess_convexity(q.P, 1e-10, 5·1024·1024, deadline)`
   (`mip_proof_relaxation.cpp:30-36`) — indeterminate or non-PSD is
   inconclusive (`numerical_failure` at the node, rejection at replay),
   never `optimal`.

## 4. Incumbent verification from original quadratic data

1. Incumbent objectives are computed from the **original** quadratic
   data: `milp::compute_objective` = `offset + cᵀx + ½xᵀ(Qx)` in long
   double (`heuristics.cpp:83-99`) feeds every incumbent update; the
   solver's `objective_value` is never stored as the incumbent.
2. The API gate recomputes independently: `verify_primal(model, {primal,
   original_objective}, …, 1e-6, …)` checks integrality, bounds, rows and
   the quadratic objective against the original model
   (`primal_verifier.cpp:102-110`, `engine_milp.cpp:71-84`); a mismatch
   is `numerical_failure`, never a presented incumbent.
3. Proof replay re-checks the recorded incumbent the same way at
   `mip_proof.cpp:61-64` before any gap or bound gate runs.
4. The two quadratic evaluations (`compute_objective` on the stored
   matrix, `verify_primal` via `make_quadratic_model` pair-averaging)
   agree because parsers store the full symmetric matrix (MPS mirrors
   off-diagonals `mps_model.cpp:66-72`, LP doubles diagonals
   `lp_model.cpp:55-66`); a hand-built single-triangle matrix fails
   closed as `numerical_failure`.

## 5. Proof replay for QP leaves

1. Replay applies the node domain **before** relaxation checking
   (`mip_proof.cpp:101`), so the overlay reaches both the row-mapped
   box and the support minimization.
2. A QP leaf must carry the expected status (`:102-105`); an infeasible
   leaf needs a verified Farkas witness, an optimal leaf needs a fresh
   PSD assessment plus an accepted KKT witness
   (`mip_proof_relaxation.cpp:25-36`) — otherwise rejected ("invalid
   convex QP KKT leaf").
3. The bound is **re-derived** from the recorded witness through the §2
   formula (`:37`); recorded objectives/primals are cross-checked, not
   trusted. The incumbent is converted to minimization space first
   (`mip_proof.cpp:65`), and a leaf must close the requested gap
   (`:109-112`) before the global bound takes the min (`:113`, F3).
4. Budgets, fingerprint tiers and status labels are exactly the
   milp-node-bounds §6 rules: exhausted ≠ accepted; stripped fingerprint
   → `replayed_tree`; mismatch → rejected; the search may keep an
   incumbent under resource stops but no global claim beyond the closed
   portion.

## 6. Scope limits and known asymmetries

1. **Cut-free proof.** The proof tree is the independent cut-free
   replay; a cut-enabled MIQP search is basis-gated off anyway (§1.2),
   so no cut obligations arise for MIQP.
2. **Tolerance asymmetry.** Search prunes at `absolute_gap_tolerance`
   (1e-6) through `prune_guard`, while proof build/replay require the
   leaf to close at `mip_proof` tolerance (1e-8, `relative_gap = 0` for
   `optimal`; `mip_certificate.cpp:60-61`, `mip_proof.cpp:109-112`).
   The stricter replay can downgrade a search-proven result to
   `Feasible`; build and replay agree with each other, so the asymmetry
   can never accept a false proof.
3. **Failure-site label.** An `unsupported` result currently reports
   `failure_site = "milp_node_lp_capacity"` for every cause
   (`engine_milp.cpp:29-33`), including an infinite-endpoint
   supporting-bound failure; the status and message stay distinct
   (`node_qp.cpp:56-60`). The label is a known reporting defect, not a
   bound defect.
4. **No QP primal bound.** `sol.objective_value` never bounds anything
   (§2.5); a node with no finite support has no finite bound, by
   design (§2.3).

## 7. Test obligations

1. **Direct bound algebra** (first direct call sites of
   `supporting_lower_bound`): every §2.2/§2.3 case — both multiplier
   signs, zero multiplier with infinite unused side, all three endpoint
   cases including infinite used endpoints, dimension mismatch, the
   stored `512ε` weakening (`returned ≤ independent long-double/rational
   recompute`), and the input-space invariant (maximize model → not
   callable / documented boundary).
2. **Independent recompute helper** in tests: a high-precision
   (rational/long-double) restatement of §2.2 used across the cases,
   never sharing code with `supporting_bound.cpp`.
3. **Node overlays**: lower- and upper-side tightening reach both the
   QP and the support minimization; infinite node endpoint →
   `unsupported` → unresolved (P5), `proven` blocked, inherited bound
   preserved; overlay through full `milp::solve` child materialization.
4. **Section 13 required cases**: `min(y−0.3)²` root/child bounds;
   `min(x−1)²+0.2y` with `x ≤ y` → 0.2 at `(1,1)`; unbounded-box but
   linearly-bounded region stays inconclusive; nearly-PSD/indefinite
   never yields `optimal`; maximize + nonzero offset reverses objective
   and bound signs.
5. **Incumbents**: a MIQP solve asserts `original_verified`; a tampered
   quadratic incumbent objective is rejected at the engine gate and at
   replay.
6. **Proof attacks on quadratic trees**: altered QP primal, objective,
   certificate, forged incumbent/split, and `claims_infeasible` against
   a QP leaf must all be rejected — zero accepted false proofs.
7. **Randomized enumeration**: seeded small integer boxes with
   quadratic objectives solved by `milp::solve` must match exhaustive
   enumeration (objective, honest bound, feasible incumbent).
8. **Benchmark**: a separate convex MIQP stratum (own record under
   `evidence/`, indexed from `evidence/INDEX.md`) — never merged into
   the linear MIP summary; statuses as measured; no speed claim.

## 8. Change procedure

Any change to this contract — the §2 formula or guards, the fail-closed
set, the status→P mapping, the incumbent or replay gates, or the scope
limits — must update this page in the same change, keep the §7 tests
green, re-run `ctest -j8`, and, when a tolerance or label moves, update
[numerical-policy.md](numerical-policy.md) under its own change
procedure. Convexity-class or KKT-witness changes defer to
[convex-qp.md](convex-qp.md); search-status changes defer to
[milp-node-bounds.md](milp-node-bounds.md).
