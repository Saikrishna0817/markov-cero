# Restricted convex MINLP outer-approximation contract (v1)

Binding contract for blueprint task MINLP-01: exactly which structurally
represented convex quadratic MINLPs are supported, how every outer-
approximation cut is derived, weakened and independently replayed, how
incumbents and master lower bounds are certified, which counters and caps
stop the loop, and the test/benchmark obligations that gate acceptance.
Authority: blueprint §15 and the MINLP-01 card.

Related contracts and evidence:

- [Local SQP contract](nlp-local-sqp.md) — fixed-integer subproblem
  semantics (§4) this loop alternates with the master.
- [Elastic restoration contract](nlp-restoration.md) — what a failed
  SQP linearization may and may not report (§5); MINLP counts those
  failures but never converts them into claims.
- [MILP node bounds contract](milp-node-bounds.md) — the only source of
  a certified master lower bound (§3.1, §6); this contract re-checks
  finiteness and consistency, it never re-derives duals.
- [Convex QP contract](convex-qp.md) — PSD screening (`qp::
  assess_convexity`) and the `0.5·xᵀPx` QUADOBJ convention (§2).
- [Numerical contract](numerical-policy.md) — status witness table and
  assurance labels (§3); global assurance stays solver-trusted here.
- `evidence/INDEX.md` — the §11 benchmark record.

Algorithmic reference: Duran–Grossmann (1986) and Bonami et al. (2008),
cited in blueprint §24. Their assumptions — convexity of `f` and every
`g_i`, and correctness of each subproblem — are restated as requirements
below, not inherited by citation.

## 1. Scope and assurance level

1. **Class.** Structurally represented convex quadratic MINLP only:
   linear + NLOBJ/QUADOBJ objective, NLCON quadratic inequalities, linear
   matrix rows, variable bounds, selected integer variables (§2).
2. **Assurance.** Experimental and **solver-trusted** for global claims:
   `canonical_verified = false`, `certificate_type =
   "incumbent_feasibility; solver_trusted_oa"` after a verified
   incumbent, `"none"` otherwise. The gap closure itself is never
   presented as independently replayed evidence (blueprint §15 step 9).
   MINLP-02 adds the full OA proof replay; until then §9 fixes what the
   API may say.
3. **Locked.** `gap_tolerance = 1e-3` (NLP accuracy bound, plan W1/D-03)
   is not changed by this contract. The bound-vs-incumbent error
   allowance is §12.
4. **What this contract never grants.** No nonconvex claim, no
   callback-only claim, no infeasibility claim from a failed subproblem,
   no unboundedness claim from an unbounded master, no big-M.

## 2. Exact structural scope

1. **Required source.** `MinlpProblem::source_model` is mandatory;
   `nlp_callbacks` are rejected (`unsupported`) because arbitrary
   callbacks expose no checkable quadratic structure (blueprint DO-NOT-DO).
2. **Screening (fail closed).** Before any cut exists:
   `require_convex_quadratic_structure` runs `source.validate()` (rejects
   out-of-range NLOBJ/NLCON term indices, dimension mismatches, non-finite
   `objective_offset` → `invalid_model`), rejects non-finite term/row
   coefficients (`invalid_model`), enforces the **512-variable cap**
   (`unsupported`, kept until data supports raising it), then requires a
   certified PSD objective Hessian after objective-sense normalization
   (`sign·H_nlobj + P` with the `0.5·xᵀPx` convention) and a certified PSD
   Hessian for every NLCON row. `non_convex` → `non_convex_minlp`;
   indeterminate (tolerance/fill exhaustion) → `unsupported`. Screening
   cost is bounded by the 512 cap; it has no separate deadline.
3. **Affine rows are exact.** `make_nlp_model` splits each matrix row
   into at most two inequality rows: `aᵀx ≤ u` and `−aᵀx ≤ −l`; an affine
   equality (`l = u = b`) becomes `aᵀx ≤ b` **and** `−aᵀx ≤ −b` — an exact
   representation of the affine set, not a relaxation. These rows enter
   the master directly and are never re-cut.
4. **Rejected.** Nonlinear equality (`n_eq > 0`, `invalid_model`) — a
   nonlinear equality is generally a nonconvex feasible set even when its
   function is convex (§15 step 8). Equality *callbacks* arrive through
   the rejected callback path. Integer-index mismatches/duplicates,
   non-finite `x0`, and `max_oa_cuts == 0` are `invalid_model` /
   `invalid_options` at entry.
5. **Not claimed, not added.** Arbitrary callbacks, nonlinear equality,
   nonconvex MINLP, >512-variable screening, big-M reformulations.

## 3. OA derivation

1. **Convexity inequalities.** For convex `f` and `g_i`, at any expansion
   point `p` (feasible or not):

   ```text
   f(x) ≥ f(p) + ∇f(p)ᵀ(x − p)
   g_i(x) ≥ g_i(p) + ∇g_i(p)ᵀ(x − p)
   ```

2. **Rows.** The master accumulates, per evaluated point `p`:
   the epigraph row `η ≥ f(p) + ∇f(p)ᵀ(x − p)` and one constraint row
   `g_i(p) + ∇g_i(p)ᵀ(x − p) ≤ 0` per NLP inequality row. Each row is a
   **relaxation**: it may admit original-infeasible points; satisfying it
   is never feasibility evidence, and no incumbent is ever accepted from
   master rows (only from §6).
3. **Validity.** Every stored row, including its numeric weakening (§4),
   is satisfied by all original-feasible points. A rounded tangent that
   would exclude a feasible integer point is unsound (card, numerical
   considerations) — §5 verifies each row against the source polynomial
   and §10 validates against feasible points.
4. **Lower-bound chain.** Master bound `L` exists only through
   milp-node-bounds certification (§7); incumbent `U` only through §6;
   `L ≤ U + allowance` is checked every iteration and a violation is
   `numerical_failure`.

## 4. Cut provenance and numeric weakening

1. **Record.** Every accepted tangent is stored as an `OaCut`
   (`include/markov_cero/minlp/oa_cut.hpp`):

   | Field | Meaning |
   |---|---|
   | `source_kind` | `objective`, `linear_upper`, `linear_lower`, `nlcon` |
   | `source_index` | source matrix row, NLCON index, or `kOaObjectiveSource` |
   | `source_maximize` | source objective sense (provenance; evaluation is sense-normalized) |
   | `point` | expansion point `p` |
   | `value` | stored `f(p)` / row value |
   | `gradient` | stored gradient at `p` |
   | `rhs` | row right-hand side (after weakening) |
   | `weakening` | outward guard actually added, `≥ 0` |

   Row form everywhere: `gradientᵀ x ≤ rhs` with
   `rhs = gradientᵀp − value + weakening`.
2. **Weakening.** `weakening = kOaCutWeakening · (1 + |gradientᵀp −
   value|)` with `kOaCutWeakening = 1e-9` (fixed in `oa_cut.hpp`).
   Weakening only moves a row outward (looser), never inward; `weakening
   < 0` is rejected by replay.
3. **Exposure.** `MinlpSolution::oa_cuts` carries the full provenance
   list (additive field; §9).

## 5. Independent replay

1. **Two paths.** Cut *creation* evaluates the NLP callback view built
   by `make_nlp_model`. Cut *replay* recomputes value and gradient from
   the **source polynomial**: `sign·(cᵀx + offset)` + `sign·Σ` NLOBJ
   terms + `0.5·P` energy for QUADOBJ, matrix rows straight from the CSR
   entries, NLCON rows straight from their term lists — a separate
   composition that shares only the representation accessors
   (`qp::make_quadratic_model` storage conversion), never the closure
   construction. Replay never trusts stored coefficients.
2. **In-loop, fail closed.** After each refinement, every newly created
   cut is replayed before the master is built. On any mismatch the solve
   stops with `numerical_failure` naming the rejected cut — no silent
   dropping, no continuing on unverified rows. `cuts_replayed` counts
   every completed check.
3. **Checks per cut** (tolerance `kOaCutReplayTolerance = 1e-8`,
   componentwise, `1 + |expected|` scaled):
   dimensions match; stored `value` equals the recomputed value;
   stored `gradient` equals the recomputed gradient; `weakening ≥ 0`;
   `rhs − weakening` equals `gradientᵀp − value` within tolerance;
   `source_kind`/`source_index` address an existing source row.
4. **Public API.** `derive_oa_cut(source, kind, index, point)` builds a
   cut from the source polynomial; `replay_oa_cuts(source, cuts)` returns
   `OaReplayReport{accepted, checked, message}`. Tests construct analytic
   cuts, corrupt them, and require rejection (§10 case H); MINLP-02
   builds the exported proof format on the same functions.

## 6. Incumbent verification

A subproblem result becomes an incumbent only if **all** hold:

1. `sub.status == optimal` (a failed/stopped SQP contributes no
   incumbent; its cuts at the evaluated point remain valid — §15 case F);
2. integer residual `|x_j − round(x_j)| ≤ 1e-6` for every integer index;
3. `nlp::verify_nlp_feasibility(nlp, point, feasibility_tolerance)`
   accepts — independent recomputation of every row and bound at `point`,
   not inferred from any solver status (the check the OA loop previously
   did with raw `constraint_violation` is replaced by this verifier);
4. the objective is finite when re-evaluated.

A local SQP KKT point is sufficient for a feasible **upper bound** under
these checks; it is never sufficient for a lower bound or any global
claim. The engine re-verifies incumbents independently of the library
(unchanged).

## 7. Master bound certification and status mapping

1. **Certified bound accepted** only when `milp::solve` returns status
   `optimal`, `gap_satisfied`, `feasible`, `iteration_limit`, or
   `resource_limit` **and** `best_bound` is finite (these statuses carry
   a bound certified by the MILP assurance layer per milp-node-bounds
   §3/§6 — early stops included, §15 step 6). The running bound is the
   max over accepted iterations; `bound_provenance` is set to
   `"milp_master_certified"` whenever a bound has ever been accepted and
   stays empty otherwise.
2. **`infeasible`.** The master is a relaxation: relaxation-infeasible
   implies original-infeasible (sound). With a verified incumbent
   present it contradicts `U` → `numerical_failure`. Without an
   incumbent it is reported as `infeasible`.
3. **`unbounded`.** Never propagated: supporting hyperplanes can leave
   the relaxation unbounded while nonlinear constraints bound the
   original. Status `iteration_limit` with an explicit inconclusive
   message; no finite epigraph bound is invented and **no big-M** is
   inserted (§15 step 7; card DO-NOT-DO).
4. **No certified bound** (any other status, or non-finite
   `best_bound`): with a verified incumbent the solve stops at status
   `feasible` with the incumbent and a message naming the inconclusive
   master — never `Optimal` (§15 step 6). Without an incumbent the
   master status passes through.
5. **Consistency and gap.** `L > U + feasibility_tolerance·max(1,|U|)` →
   `numerical_failure`. Relative gap `max(0, U − L)/max(1,|U|) ≤
   1e-3` (LOCKED) with both finite → `optimal`, still solver-trusted (§1).
6. **Retained-incumbent exits.** Every early exit that keeps an
   incumbent (deadline, master failure, no-bound stop, missing/non-integral
   master primal, cap stop) also carries `x`, `objective`, the last
   certified `best_bound`, and `relative_gap` when both bounds are finite.

## 8. Counters, caps and stops

1. **New counters** (all additive `MinlpSolution` fields):
   `sqp_calls` (total subproblem solves), `sqp_failures` (total failed
   subproblem calls; the existing consecutive-failure abort at >3 is
   unchanged), `master_nodes` (sum of `milp` `nodes_explored` over
   all master solves, certified or not), `cuts_replayed` (§5.2), `oa_cuts` (§4.3),
   `bound_provenance` (§7.1).
2. **OA row cap.** `MinlpOptions::max_oa_cuts` (default 10000). When the
   next refinement would exceed it, OA stops exactly like the iteration
   limit: incumbent and last certified bound retained, message names the
   cap, no optimality claim. `max_oa_cuts == 0` is `invalid_options`.
3. **Unchanged stops.** Deadline → `resource_limit`; `max_iterations`
   → `iteration_limit`; master node/time limits surface as §7 statuses;
   SQP failure abort after >3 consecutive failures (cuts accumulated
   before the abort remain valid records).
4. **Engine exposure.** The MINLP engine branch reports
   `lp_iterations = iterations`, `cuts_generated = cuts_added`, and
   `nodes_explored = master_nodes` (the SQP branch keeps its existing
   value). Benchmark counters that have no `SolveResult` field
   (`sqp_calls`, `sqp_failures`) are read from `MinlpSolution` by the
   §11 benchmark.

## 9. Interfaces and assurance exposure

1. `minlp::solve_minlp` is preserved; `MinlpOptions` and
   `MinlpSolution` grow additively; `oa_cut.hpp` is new public API.
2. `bound_provenance` distinguishes "bound certified by the master
   assurance layer" from "no bound"; it is the library-level answer to
   the card's *expose bound provenance* requirement.
3. Solver-trusted versus independently replayed: until MINLP-02, the
   only independently replayed artifacts are incumbents (§6, engine) and
   individual cuts (§5); the **global** OA/master evidence is
   solver-trusted and labeled so (`certificate_type`, engine
   `original_message`). No exported proof format ships in MINLP-01
   (§15 step 9).

## 10. Test obligations

Cases map to blueprint §15 "Required MINLP cases" (A–H) plus the card's
oracle and corruption requirements. Existing `minlp_basic_test` coverage
for B–E is retained; new tests live in `tests/minlp01_oa_test.cpp` and
`tests/minlp01_cut_replay_test.cpp` (300-line cap respected).

- **A — analytic optimum and tangent validity.** `x∈[0,2]`,
  `y∈{0,1}`, `min (x−1)² + 0.2y`, `x² ≤ y`: solve → `optimal`,
  objective `0.2` (±1e-6), `x ≈ (1,1)`. Tangents at `x = 0, 0.5, 1`
  (from `derive_oa_cut` and from a full solve's `oa_cuts`) never exclude
  feasible original points: for every stored cut and sampled feasible
  point, the row holds at the point and `gradientᵀx − rhs ≤ value_at_x`
  for the matching source function (underestimator property).
- **B — affine equality exact** (existing test): a matrix equality is
  represented as two inequalities and the optimum matches the analytic
  one; equality *callbacks* remain rejected (`invalid_model`).
- **C — nonconvex rejected** (existing): concave objective and
  `-x² ≤ 0` → `non_convex_minlp`, never a global answer.
- **D — maximize + offset** (existing): normalized internal bound and
  user-facing sign/offset agree through the API.
- **E — unbounded master inconclusive** (existing): master unbounded →
  no original-unbounded claim, no big-M.
- **F — SQP failure honesty.** A structurally convex model whose
  subproblem linearization is primal-infeasible (the infeasible-pair
  shape) → the failed call counts in `sqp_failures ≥ 1`, its cuts at
  the evaluated point remain valid records (`cuts_replayed ==
  cuts_added`), and the OA master — affine rows are exact — proves
  `infeasible`; `integer_feasible == false` with no `x` and no
  certified bound (`best_bound` non-finite).
- **G — master limit keeps verified incumbent and bound only.** A model
  whose continuous optimum is integral, solved with `milp_max_nodes = 1`
  (and the existing deadline test): incumbent retained and §6-reverified,
  `best_bound` finite and `≤ objective + allowance`, and `optimal` only
  if the LOCKED gap actually closed.
- **H — corrupted tangent rejected.** Valid cuts from `derive_oa_cut`;
  each of a corrupted gradient component, corrupted value, corrupted rhs
  and a negative weakening is rejected by `replay_oa_cuts`; the valid
  cut passes.
- **Exhaustive oracle.** For A and a second two-integer model
  (`min (x−1)² + 0.3y₁ + 0.5y₂`, `x² ≤ y₁+y₂`, binaries): enumerate all
  integer assignments, take the analytic continuous optimum per
  assignment, and require the OA result to match the enumeration minimum
  within 1e-6 with `bound ≤ oracle + allowance`.
- **Random feasible-point validation.** Seeded `std::mt19937` (no
  `rand()`): sample feasible points of A, require no stored cut excludes
  them (row holds within the §12 allowance) — the acceptance criterion
  "no invalid tangent in analytic/random validation".
- **Caps/counters.** `max_oa_cuts` small → stop names the cap, cuts
  never exceed it, incumbent still honest; `sqp_calls`, `master_nodes`,
  `cuts_replayed`, `bound_provenance` asserted non-zero/non-empty
  wherever §8 defines them.

## 11. Benchmark obligation

1. **Separate stratum.** `scripts/bench_minlp01_oa.cpp` (CMake target
   `minlp01_oa_benchmark`) solves embedded restricted convex quadratic
   MINLP cases with known optima directly through `minlp::solve_minlp`
   (the counters live in `MinlpSolution`). One dedicated record
   `evidence/minlp01-oa-<date>.json`, listed in `evidence/INDEX.md`,
   never merged with the MIP/MIQP/NLP summaries.
2. **Reported per case:** status, message, objective, `best_bound`,
   `bound_provenance`, `relative_gap`, `iterations`, `sqp_calls`,
   `sqp_failures`, `cuts_added`, `master_nodes`, `cuts_replayed`, wall
   milliseconds. Statuses as measured; **no speed claim** and no claim
   beyond §1.

## 12. Numerical allowances

| Quantity | Value | Where |
|---|---|---|
| Cut weakening | `1e-9 · (1 + |exact rhs|)`, outward only | `kOaCutWeakening`, §4.2 |
| Replay tolerance | `1e-8 · (1 + |expected|)` componentwise | `kOaCutReplayTolerance`, §5.3 |
| Integer residual (incumbent) | `1e-6` | §6.2 |
| Incumbent feasibility | `feasibility_tolerance` (default `1e-6`) | §6.3 |
| Bound vs incumbent allowance | `feasibility_tolerance · max(1,|U|)` | §7.5 |
| Relative gap (LOCKED) | `1e-3` | §1.3 |
| PSD screening tolerance | `1e-10` (existing `assess_convexity` argument) | §2.2 |

## 13. Limits and change procedure

1. This contract grants no global certificate; MINLP-02 supersedes §9.3
   when full OA/master replay lands. Do not widen scope by name: raising
   the 512 cap, accepting callbacks, or changing the locked gap requires
   a contract revision with data, in the same change as the code.
2. A change to cut math, weakening, status mapping, caps, or counters
   must update this page together with the code (numerical-policy §8
   change procedure); MINLP-01 tests are the executable form of §10.
