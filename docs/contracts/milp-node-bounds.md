# MILP node bounds and proof obligations (MIP-01)

Status: binding, v1. Owner: MIP-01 slice of
[`INDUSTRY_GRADE_IMPLEMENTATION_BLUEPRINT.md`](../project/INDUSTRY_GRADE_IMPLEMENTATION_BLUEPRINT.md)
(task MIP-01, "Certify MILP node bounds and proof obligations").

Related contracts and evidence:

- [`numerical-policy.md`](numerical-policy.md) — assurance labels, MIP proof
  tolerance gate, status semantics that this contract does not restate.
- [`sparse-lp-path.md`](sparse-lp-path.md) — the LP dispatch the node
  relaxations rely on.
- [`resource-limits.md`](resource-limits.md) — cooperative stops that must
  never become proofs.
- `evidence/INDEX.md` — benchmark records for the MIPLIB obligation (§7).

Scope: the serial and parallel MILP branch-and-bound searches
(`src/milp/*`), their node-bound provenance, prune reasons, branch
partitions, cut obligations, and how they feed
`verify::build_mip_proof` / `verify::mip_proof` replay. Non-goals:
branching-heuristic quality, cut strength, MIQP internals (shared rules
here apply through `node_qp.cpp`), and proof-file format evolution
(`kMipProofFormatVersion`).

## 1. Space, sense, and bound semantics

1. Every node bound lives in the **search's minimization space**.
   Maximization inputs are normalized (objective and offset negated) before
   any relaxation runs — `search_initialize.cpp:36-52`, parallel equivalent
   in `parallel_tree_search_solve_parallel.cpp` — and the sign is restored
   exactly once at exit.
2. `NodeLpResult.lower_bound` means: a finite lower bound on the integer
   optimum of the node's subtree in minimization space, or
   `-inf` = "unknown". Unknown never prunes and never raises a bound.
3. Composition rules, enforced at every site:
   - **(F1) Finite guard**: a bound-prune comparison fires only when
     `std::isfinite(bound)`. NaN and `-inf` fall through to "not prunable";
     a non-finite value may never enter an aggregate by accident.
   - **(F2) Monotone merge**: a node's bound only rises —
     `node->lower_bound = max(parent, new)` (`search_node_relaxation.cpp:88`,
     parallel `parallel_tree_search.cpp:83`); propagation raises carry a
     `propagation` obligation (`search_node_relaxation.cpp:39-42`).
   - **(F3) Aggregation**: the reported tree bound is the minimum over open
     frontier bounds, dropped-capacity bounds and unsolved-node bounds
     (`search_context.hpp:91-105`, `search_finish.cpp`), never a maximum
     over them.

## 2. Prune reasons and required witnesses

Every site that removes a node from the live search must appear in this
table. A reason not listed here is a contract violation.

| # | Reason | Required witness | Serial site | Parallel site | Accounting when sound |
|---|---|---|---|---|---|
| P1 | Bound prune (inherited or post-LP) | finite, certified weak-dual node bound ≥ incumbent − `absolute_gap_tolerance`, compared through the §3.1 prune-time guard | `milp_solver.cpp:41-43`, `search_node_relaxation.cpp:91-93` | `parallel_tree_search.cpp:44-49,86-91`, lazy cut-off `work_queue.cpp` | none needed; loss = 0 |
| P2 | Relaxation infeasible | verifier-accepted Farkas ray (`certify_result` → `verify_sparse_result`) | `search_node_relaxation.cpp:55-59` | `parallel_tree_search.cpp:72-75` | none; subtree empty |
| P3 | Near-integral gap prune | rounded point accepted by `check_integer_feasibility` **and** `|LP obj − rounded obj| ≤ absolute_gap_tolerance`; loss ≤ `absolute_gap_tolerance` (edge case "gap satisfied but nonzero") | `search_branch.cpp:12-22` | `parallel_tree_search.cpp:118-122` | incumbent updated first |
| P4 | Exact-integral but unroundable | **not prunable** — counted unresolved | `search_branch.cpp:25-30` | `parallel_tree_search.cpp:124-129` | `unsolved_node_lps` / `unresolved_node_lps` + bound folded into `min_unsolved_bound` |
| P5 | Non-optimal node LP | **not prunable** — one cold retry, then counted unresolved; bound stays | `search_node_relaxation.cpp:60-85` | `parallel_tree_search.cpp:76-80` | same as P4; blocks `proven` (`search_finish.cpp`) |
| P6 | Branch selector finds no variable while the point is fractional, or the split is degenerate (`⌊v⌋ ≥ ⌈v⌉`) | **not prunable** — internal failure; counted unresolved and bound folded in | `search_branch.cpp` (both guards, §4.2) | `push_branch_children` returning `split_rejected`, handled in `parallel_tree_search.cpp` | same as P4 |
| P7 | Queue capacity drop | dropped bound folds into `min_lower_bound()`; capacity blocks `proven` | `search_context.hpp:63-66` | `work_queue.cpp` capacity path | `capacity_exhausted` → never `proven` |
| P8 | Resource stop (deadline, memory refusal, cancellation, node quota) | stop reason only; never a proof | `milp_solver.cpp:25-35`, search phases | `parallel_tree_search.cpp:200-219` | `resource_limit` + `stop_reason` |
| P9 | Relative-gap break | finite tree bound + incumbent | `milp_solver.cpp:60-65` | `parallel_tree_search.cpp:225-239` | `gap_satisfied` vs `optimal` per `gap.hpp` |
| P10 | Cut re-solve failure | cuts reverted or never applied; **never a prune and never a proof** | `search_root_relaxation.cpp`, `search_separate_cuts.cpp` | `parallel_tree_search_root_cuts.cpp` | cuts not recorded as applied |
| P11 | Strong-branch probe infeasible | verified child-probe infeasibility | `search_branch.cpp:79-81` | n/a | node pruned with both children empty |
| P12 | Split gates both reject (empty integer domain) | both `evaluate_split` gates fail: `⌊v⌋ < parent_lower − 1e-9` and `⌈v⌉ > parent_upper + 1e-9`, so `(⌊v⌋, ⌈v⌉)` contains no integer of the parent interval | `search_branch.cpp` (`++empty_domain_nodes`) | `push_branch_children` (`queue.note_empty_domain()`), root push and worker path | `empty_domain_nodes` counter; conclusive emptiness — never a bound claim, never blocks `proven` |

A "silent" node death — leaving the queue with no counter, no bound fold
and no reason — is forbidden (P4-P7 exist precisely to close that hole).

## 3. Node lower bound rules

1. **Provenance per path** (the "dual/support witness" rule — a
   solver-reported primal objective is never itself a certified bound):
   - **Sparse PDLP**: the verifier's weak-dual certificate
     (`verify_linear_solution` → `certificate.bound`), required finite,
     moved downward by `1e-10·(1+|bound|)` at storage time
     (`node_lp.cpp:102-104`).
   - **Simplex paths** (warm dual, cold reference, and the certified retry;
     `node_lp.cpp` `certified_dual_bound` at three assignment sites): the
     bound is the **dual objective** `offset + rhsᵀy` of the certified
     canonical dual solution (the same quantity the independent verifier
     compares at `sparse_lp_verifier.cpp:117-118`), accumulated in long
     double and mapped into search space by
     `transform::reconstruct_objective`. Non-finite ⇒ `-inf` (no bound, no
     prune). The primal objective may not be assigned to `lower_bound`.
   - **Where the downward guard lives**: the `1e-10·(1+|bound|)` guard is
     applied at **prune time** (`gap.hpp` `prune_guard`, called from every
     P1 comparison), never subtracted from the stored value — a relative
     storage guard of that size exceeds `absolute_gap_tolerance` (1e-6)
     once objectives reach ~1e4, silently converting `optimal` into
     `gap_satisfied`. Gap decisions, aggregates and reported bounds see
     the certified value itself.
   - **QP nodes**: `qp::supporting_lower_bound` with a finiteness check
     (`node_qp.cpp`).
   - **Inherited / child seeds**: the parent's certified bound
     (`search_branch.cpp`, `work_queue.cpp`); root seeds from the certified
     root relaxation.
2. **Certification precondition**: only a `certify_result`-accepted
   `optimal` result may contribute a bound at all; rejected witnesses
   become `numerical_failure` and fall under P5.
3. **Guards**: F1 plus the §3.1 prune-time guard at every prune comparison
   (both engines), F2 merge, and
   `min_unsolved_bound` is initialized to `+inf` (not `0`) so an unsolved
   node with a positive bound cannot drag the reported bound below its own
   value while an all-positive frontier reports its true minimum.
4. **Sense**: because the search is normalized (§1.1), simplex weak-dual
   bounds are computed only for minimization-space models (`objective_sign
   == +1`); a non-normalized caller receives `-inf` rather than a
   bound whose direction might flip under
   `transform::reconstruct_objective`.

## 4. Branch partition

1. **Split rule**: for a fractional value `v` (so `floor(v) < ceil(v)`),
   the down child tightens the variable to `≤ floor(v)` and the up child to
   `≥ ceil(v)`; both children inherit the parent domain otherwise. The
   construction is one shared module — `evaluate_split` and
   `push_branch_children` in `branch_partition.cpp`, called by the serial
   search (`search_branch.cpp`) and both parallel push sites. Every integer
   point of the parent domain lies in exactly one child: the integer
   interval between `floor(v)` and `ceil(v)` is empty.
2. **Production invariants** (checked by `evaluate_split` for both engines):
   `floor(v) ≥ parent_lower − 1e-9` and `ceil(v) ≤ parent_upper + 1e-9`
   gate each child; `floor(v) < ceil(v)` must hold or the split is rejected
   as an internal failure (P6: counted unresolved, bound folded into
   `min_unsolved_bound`). A child that fails its gate is **not pushed and
   not silently swallowed**: when both gates reject, `push_branch_children`
   records the empty domain (`queue.note_empty_domain()`, serial
   `++empty_domain_nodes`) and reports `ChildPushStatus::empty_integer_domain`;
   the counter surfaces as `Result::empty_domain_nodes` (P12). Empty-domain
   nodes block no claim: the subtree provably holds no integer point.
3. **Independent authority**: the proof replay re-derives children domains
   and rejects any split that is not an integer partition
   (`mip_proof.cpp`, "invalid integer partition"; domains in
   `mip_proof_internal.hpp`). Production construction must agree with the
   replayed rule; the adversarial suite (§7.1) asserts both engines'
   children satisfy the replay checker's split conditions. The exhaustive
   certificate over a grid of split values and parent domains lives in
   `tests/milp_branch_partition_test.cpp` (`milp_branch_partition`).

## 5. Cut obligations

1. Cuts are **audit annotations, never replay checks**: the independent
   proof is cut-free (`mip_proof.hpp:21-25`). Recorded cut obligations
   (`record_optimizer_cut_notes`, `search_context.hpp:39-54`) carry
   coefficients, rhs and `observed_lhs`. Replay ignores their content but
   validates their structure — out-of-domain or non-finite obligation rows
   are rejected (§7.1), so a tampered artifact cannot smuggle malformed
   notes past the checker.
2. **Provenance parity**: `observed_lhs` is evaluated from the **pre-cut**
   node primal in both engines (serial `search_root_relaxation.cpp`,
   `search_separate_cuts.cpp`; parallel `parallel_tree_search_root_cuts.cpp`
   records inside the round loop, before the re-solve is committed). An
   obligation whose lhs reflects a different point than the one separation
   reacted to is a contract violation.
3. **Validity rule**: a GMI/MIR row generated for a structural integer
   basic variable must be derived under a canonicalization that preserves
   that variable's integer lattice (integral shift, unit scale — the
   canonical row and `f0` must live in the same units as the original
   integer lattice). The same lattice condition is checked for **every
   integer column the row touches** (`cut_lattice.hpp`,
   `cut_row_lattice_preserving`), not only the basic variable. When the
   canonicalization does not preserve the lattice for the basic variable
   or an active integer contributor, the generator **skips** the row; it
   may not emit a row whose derivation mixes original-unit `f0` with
   canonical-unit coefficients outside those conditions. Validity is
   demonstrated by enumerated-point tests (§7.5), including non-integral
   variable bounds and upper-bound-only integer variables.
4. **Rejection paths**: non-finite or violated rows are filtered before
   admission (`gomory.cpp`, `cut_pool.cpp`); recorders drop obligations
   whose lhs/rhs is non-finite or unviolated at the recorded point
   (`search_context.hpp`, `parallel_tree_search_internal.hpp`); a cut
   re-solve that fails reverts the cuts (P10) — a rejected cut never
   prunes, never enters the proof, and never changes a reported bound.

## 6. Proof obligations, fingerprint, and result labels

1. **Proof scope**: `build_mip_proof` generates an independent, cut-free,
   original-domain tree; `verify_mip_proof` re-derives every domain,
   partition and leaf witness without calling a solver
   (`mip_proof.hpp:90-96`). Optimizer obligations attached at build time
   (`mip_certificate.cpp:86`) are audit annotations and carry the model
   fingerprint with them; the replayed artifact reports
   `independent_tree` when the fingerprint is bound and `replayed_tree`
   otherwise (`mip_proof.cpp:101-108`).
2. **Bounded trees and resource stops**: budgets (time/node/witness
   limits) are first-class — a budget-limited build or replay reports
   `exhausted` with its `MipProofBudgetKind` and never `accepted`; a search
   stopped by a resource limit (P8) keeps `resource_limit` status and may
   only carry a proof over the portion it actually closed.
3. **Result labels** (this contract fixes the mapping; assurance labels
   stay owned by `numerical-policy.md`):
   - `Optimal` — search proven (exhausted frontier with zero unresolved
     node LPs, or gap closed within tolerance) **and** an accepted proof
     **and** the independent primal/integrality check passed; otherwise the
     engine downgrades to `Feasible`
     (`engine_milp.cpp:99-106`).
   - `GapSatisfied` — proven within `relative_gap_tolerance` with a finite
     bound but not exact; still requires the accepted proof to survive
     unchanged, else downgraded.
   - `Feasible` — an independently verified incumbent exists; optimality
     was not certified (tree incomplete, unresolved nodes, proof rejected).
   - `Unverified` — neither incumbent nor infeasibility certified; never
     presented as a solution (`assurance = unverified`).
   - `Infeasible` — frontier exhausted with zero unresolved nodes and no
     incumbent, or an accepted infeasibility proof; unresolved nodes make
     infeasibility `resource_limit`, never proven.
4. **No unverified prune**: no prune in §2 may fire without its listed
   witness, and no status above `Feasible` may be reported without an
   accepted proof. Solver-reported node objectives never stand in for
   certified bounds (§3.1).

## 7. Test and benchmark obligations

1. **Adversarial proof suite**: corrupted node bounds (overstated bound
   claiming a false prune), corrupted cuts and splits (aliasing, cyclic,
   fractional, out-of-domain), tampered incumbent, tampered fingerprint,
   and empty/dual-broken leaves must all be **rejected** by
   `verify_mip_proof` — zero accepted false proofs.
2. **Exhaustive small integer programs**: fixed and seeded small
   binary/integer models solved by the production search must match an
   in-test brute-force enumeration exactly (objective, bound consistency,
   independently feasible incumbent).
3. **Edge cases** (blueprint list): timeout **with** incumbent keeps a
   verified primal and an honest (never overstated) bound; `gap_satisfied`
   with nonzero gap; no incumbent (infeasible vs resource-limit
   distinction); queue capacity (P7); weak/unknown PDLP node bound (F1);
   nearly integral branch value (P3/P4); cut rejected after re-solve
   (P10) leaves statuses and bounds unchanged.
4. **Partition tests**: production child construction (serial and
   parallel) is checked against the replay split rules, including the
   both-children-rejected and branch-selector-failure paths (counters
   present, `proven` blocked for P6).
5. **Cut validity**: generated GMI/MIR/cover rows are tested against the
   complete enumerated integer-feasible point set of small models —
   including an integer variable with a non-integral lower bound and an
   upper-bound-only integer variable — with zero integer points cut off.
6. **Benchmark**: a MIPLIB subset runs through the production CLI with
   status, node count, reported bound, gap and every complete failure
   recorded in `evidence/` and indexed from `evidence/INDEX.md`. The
   record states statuses as measured; it makes no speed claim.

## 8. Change procedure

Any change to this contract — a new or removed prune reason, a changed
bound provenance or guard, a changed partition rule, a changed cut-validity
rule, or a changed status mapping — must update this page in the same
change, keep the tests named in §7 green, re-run `ctest -j8`, and, when
assurance derivation or status semantics move, update
[numerical-policy.md](numerical-policy.md) under its own change procedure.
The counters, statuses and obligation kinds named here are part of the
contract; renaming them requires a version bump.
