# Convex MINLP outer-approximation proof replay contract (v1)

Binding contract for blueprint task MINLP-02: the exact proof object that
makes a global convex MINLP claim independently checkable, every replay
obligation the verifier must re-derive from the source model, the build
and budget rules, the tier/certificate/status map (including the
`oa_replayed` assurance label reserved by `numerical-policy` §3), and the
test/benchmark obligations that gate acceptance.
Authority: blueprint §15 and the MINLP-02 card.

Related contracts and evidence:

- [MINLP OA contract](minlp-oa.md) — the solver loop, cut derivation and
  the solver-trusted bound this task must independently certify (§7).
- [MILP node bounds contract](milp-node-bounds.md) and the MIP proof
  framework (`verify/mip_proof.hpp`) — the embedded master tree proof is
  a `MipProof` over the rebuilt OA master; its obligations are unchanged.
- [Convex QP contract](convex-qp.md) — `qp::assess_convexity` PSD
  classification reused as a mathematical primitive (§3 O3).
- [Numerical contract](numerical-policy.md) — reserves `oa_replayed`
  for exactly this acceptance condition (§5.4).
- `evidence/INDEX.md` — the §8 benchmark record.

Algorithmic basis: a valid tangent of a convex function is a global
underestimator, so every OA master (integer) optimum lower-bounds the
original MINLP optimum; an independently replayed, exhaustive,
cut-free branch-and-bound tree of the final master therefore certifies
`L ≤ OPT`, and a source-verified integer incumbent certifies
`OPT ≤ U`. Acceptance requires both with `U − L` inside the locked gap.
Floating-point replay with guarded tolerances is **not** a formal exact
proof; it is an independent recomputation with conservative inequality
guards, and every accepted claim is labeled with the tolerance set in
§10.

## 1. Scope and assurance level

1. **Class.** Exactly the structurally represented convex quadratic
   MINLPs accepted by `minlp-oa.md` §2 (≤512 variables, no callbacks).
   A proof is built only for solve statuses `optimal`, `gap_satisfied`
   or `infeasible` coming out of the OA loop with a captured master
   (§6.1); every other status produces no proof and never a global
   claim.
2. **Assurance upgrade.** This contract is the only way a MINLP solve
   reaches `canonical_verified = true`. Acceptance (§5) emits
   `assurance = "oa_replayed"`; anything less stays
   `original_primal_checked` or `unverified`, exactly as
   `minlp-oa.md` §1.2 states for the pre-MINLP-02 world.
3. **Locked, unchanged.** `gap_tolerance = 1e-3`, the incumbent gate
   (`minlp-oa.md` §6), the convexity screening and the no-big-M rule.
   This contract adds proof replay on top; it never loosens a solve-side
   check.
4. **What this contract never grants.** No `canonical_verified=true`
   without a completed `verify_oa_proof` acceptance; no solver call
   inside replay; no nonconvex, callback, unbounded or big-M claim; no
   `Optimal` without an accepted independent replay (§5.3).

## 2. Proof object and format

1. **Record** `verify::OaProof` (public header
   `include/markov_cero/verify/oa_proof.hpp`), fields:
   - `format_version` (uint32, starts at 1);
   - `model_fingerprint` — `std::to_string(hash_model(source).
     fingerprint())`, ≤ `kOaProofFingerprintLimit = 4096` bytes;
   - `source_sense` (`minimize`/`maximize`) and `kind`
     (`optimal`/`infeasible`);
   - `convexity_pivots[]` — `minimum_pivot` reported by
     `qp::assess_convexity` for the objective Hessian (sense-signed)
     followed by one entry per NLCON Hessian, in source order;
   - `cuts[]` — the `minlp::OaCut` records verbatim (provenance of
     every tangent: kind, index, sense flag, point, gradient, value,
     rhs, weakening);
   - `master_history[]` — one `minlp::MasterRecord` per OA master
     solve: iteration index, master status, raw master bound,
     certified flag, cumulative cut count at that master;
   - `claimed_objective`, `claimed_best_bound`,
     `claimed_relative_gap` — source-sense values of the solve result;
     for `kind == infeasible` the writer emits exact zeros and the
     reader demands them (there is no bound or objective claim);
   - `incumbent[]` — the source integer incumbent; empty iff
     `kind == infeasible`;
   - `master_proof` — an embedded `verify::MipProof` over the rebuilt
     master (§3 O5–O6), serialized as a length-prefixed block so the
     MIP reader's trailing-data rule applies inside the block only.
2. **Serialization** `write_oa_proof` / `read_oa_proof`: whitespace
   token text, `std::setprecision(17)`, first line
   `MARKOV_OA_PROOF 1`. Field order follows §2.1; vectors are
   length-prefixed; the fingerprint is `<len> <raw bytes>`; the
   embedded MIP proof is `<byte_len> <bytes>`. The reader rejects:
   wrong magic or version (strict equality in writer, reader and
   verifier), out-of-range `kind`/`source_sense`/cut-kind tags,
   non-finite scalars, dimension mismatches between stored lengths and
   data, `cuts.size() > options.maximum_tangents`, fingerprint over
   the limit, budget-record inconsistencies inside the embedded MIP
   block, deadline expiry while parsing, and any trailing data after
   the final field.
3. **What is *not* in the record.** The master model itself (replay
   re-assembles it from the source, §3), solver wall-clock, node
   telemetry of the OA loop beyond the history, and audit obligations
   of the embedded tree (optional in the MIP format; the builder
   passes an empty trailer).

## 3. Replay obligations (accept iff every obligation holds)

The verifier `verify_oa_proof(source, proof, options)` performs no
optimization. It accepts only if all of the following pass, in order;
the first failure stops with `rejected` and a message naming the
failed obligation.

- **O1 — structure.** Reader-level rules of §2.2 (already enforced on
  parse; re-checked when the record is built in memory).
- **O2 — fingerprint and sense.** `proof.model_fingerprint` equals the
  recomputed source fingerprint, and `proof.source_sense` equals the
  source model's objective sense (a flipped sense would misinterpret
  every source-sense claim). Empty fingerprint ⇒ accepted with
  tier `replayed_oa` (never canonical, §5). Any mismatch ⇒
  `rejected` (stale or forged model binding).
- **O3 — convexity.** Independently assemble the objective Hessian
  from `objective` + `nlobj_terms` with the source sense sign and
  every NLCON Hessian from its terms (`1e-10` assessment tolerance,
  assembly implemented in `src/verify/oa_proof_internal.hpp`, not by
  calling the minlp screening helper), run `qp::assess_convexity`, and
  require `positive_semidefinite` for each; then cross-check every
  recomputed `minimum_pivot` against the stored value within
  §10 P1 (evidence tamper check; soundness comes from the
  recomputation, not the stored number).
- **O4 — tangents.** `minlp::replay_oa_cuts(source, proof.cuts)` —
  every cut re-derived from the source polynomial at its recorded
  point within `kOaCutReplayTolerance = 1e-8`, with sense flags
  consistent with `source_sense` and weakening rules of `minlp-oa.md`
  §4–§5. Missing, duplicated, altered or out-of-range cuts fail here
  or at O5.
- **O5 — master assembly binding.** Re-assemble the master model from
  `proof.cuts` using the verifier's own assembly (epigraph column
  `eta`, objective rows before constraint rows, `oa_row_k`/`xj`/`eta`
  names, source bounds, integer marks, minimize sense — mirroring the
  solve-side layout from source data only). The embedded
  `master_proof.model_fingerprint` must equal the recomputed
  fingerprint of this assembled master. This binds every row, bound,
  name and integer mark of the certified master to source-derived data
  without shipping the model.
- **O6 — master tree.** `verify_mip_proof(assembled_master,
  proof.master_proof, mip options)` — the embedded cut-free tree
  verifies (LP/convex-QP leaf witnesses, domain partition, incumbent
  of the master, no solver call) and yields a certified lower bound
  `L*`; its budget must not be exhausted.
- **O7 — bound consistency.** `proof.claimed_best_bound` equals `L*`
  within §10 B1 (source-sense compared), and the history obeys:
  certified entries only with statuses from `minlp-oa.md` §7.1's
  certified set, certified bounds finite and non-decreasing, and the
  last certified entry equal to `claimed_best_bound` within §10 B1.
  Non-certified history entries are disclosure only (any finite bound,
  any valid status enum value, still non-decreasing when certified).
- **O8 — incumbent.** Dimensions match the source; every discrete
  variable integral within §10 I1; source feasibility re-checked
  through the shared NLP verifier — all linear and nonlinear rows plus
  variable bounds — at §10 F1, mirroring the `minlp-oa.md` §6 gate;
  objective re-evaluated from the source (normalized, then
  sense-mapped) equals `claimed_objective` within §10 O1. An
  incumbent present with `kind == infeasible`, or absent with
  `kind == optimal`, is rejected.
- **O9 — claim.** `kind == optimal`: recompute
  `gap = max(0, U − L*) / max(1, |U|)` in source sense and require
  `gap ≤ 1e-3` (locked) and `gap ≤ claimed_relative_gap + §10 G1`;
  require `L* ≤ U + §10 B2` (the bound never exceeds the incumbent
  beyond tolerance). `kind == infeasible`: the embedded MIP proof's
  `claims_infeasible` must be true and `incumbent` empty.
- **O10 — kind consistency.** `kind == optimal` ⇔ embedded
  `claims_infeasible == false`; claimed fields zero/empty exactly as
  §2.1 requires for `infeasible`.
- **O11 — caps.** Cuts ≤ `maximum_tangents`; embedded tree within
  `maximum_nodes`/`maximum_witness_values`; replay inside the shared
  deadline (§4.2). Exhaustion ⇒ `status = exhausted`, never accepted.
- **O12 — no fallback.** Replay never solves, repairs or re-derives a
  bound by optimization. The only numbers it trusts are freshly
  recomputed ones; the only bound it publishes is `L*` from O6.

## 4. Build path (generator side)

1. **Entry.** `api::detail::certify_minlp(model, options, out,
   result, ctx, sol)` (new `src/api/minlp_certificate.cpp`, declared
   next to `certify_mip`), called from the MINLP branch of
   `engine_nonlinear.cpp` after incumbent verification and before the
   downgrade check. Gate: `options.enable_mip_proof == true` and
   `result.status ∈ {optimal, gap_satisfied, infeasible}` — the same
   switch and budget fields as `certify_mip` (§6.2). Disabled ⇒ no
   proof object; `proof_status` stays `not_requested`.
2. **Budgets and stops.** `enable_mip_proof`,
   `mip_proof_time_limit_seconds` (default 5 s), `mip_proof_max_nodes`
   (10000), `mip_proof_max_witness_values` (4e6) are **shared with the
   MIP proof family** — the certified artifact is a MILP tree, and one
   budget family keeps one CLI/Python surface. The proof deadline is
   clipped to `lp_options.deadline`; pre-start expiry ⇒
   `proof_status = "exhausted"`, `proof_budget_kind = "time_limit"`.
   Estimated proof bytes are charged to the solve memory budget
   (`charge_or_fail`) before building; refusal ⇒ `resource_limit`.
   Stages: `proof_build`, `proof_replay`, each with shared-stop polls.
3. **Build.** `verify::build_oa_proof(source, sol, options,
   fingerprint)`:
   - fails with `rejected` if `sol.master_history` is empty (no
     master was captured — the only statuses gated in §4.1 always have
     one; this is fail-closed defensive);
   - records `source_sense`, `kind`, source-sense claimed values
     (sense-mapped from `sol`), incumbent (`sol.x`; forced empty for
     `infeasible`), history verbatim, cuts verbatim from
     `sol.oa_cuts` (the in-loop fail-closed replay of `minlp-oa.md`
     §5.2 already guaranteed their provenance at creation);
   - computes `convexity_pivots` with the §3 O3 verify-side assembly;
   - rebuilds the master with the **verifier-side assembly of O5** on
     cuts re-derived from the source (not `sol.master_model`), then
     embeds `build_mip_proof(assembled_master, sol.master_primal,
     sol.master_objective, kind == infeasible, mip options,
     fingerprint_of(assembled_master), {})`. Building against the
     re-derived master is what makes build and replay byte-identical:
     both sides derive rows from the source through `derive_oa_cut`.
   - Exceptions (`bad_alloc`, `length_error` rethrown) map to
     `proof_status = "rejected"` with build-time attribution.
4. **Replay.** Under stage `proof_replay`, call `verify_oa_proof`,
   copy the report into the generic `SolveResult.proof_*` fields
   (mirroring `copy_proof_report`; `proof_format_version` = OA
   version, `proof_model_fingerprint` = source fingerprint), attach
   `out.oa_proof` and the build/replay millisecond fields, then apply
   §5.

## 5. Tiers, certificates, status map

1. **Tiers.** `independent_oa` (fingerprint matched, all obligations
   held), `replayed_oa` (accepted with an empty fingerprint — a
   stripped record: structurally and mathematically valid but not
   bound to this model), `unverified` (rejected, exhausted, disabled
   or errored).
2. **Canonical and certificate strings.**
   - `accepted + independent_oa + !budget_exhausted` ⇒
     `canonical_verified = true`; `certificate_type =
     "incumbent_feasibility; independent_oa_gap"` (or
     `...; independent_oa_tree` when `claimed_relative_gap == 0`);
     `out.best_bound` and `out.relative_gap` are refreshed from the
     replay (`sense_sign * L*` and the recomputed gap) — the published
     bound is the replayed one, never the solver's;
   - `accepted + replayed_oa` ⇒ `canonical_verified = false`,
     `certificate_type = "replayed_oa_bound"`;
   - anything else ⇒ flags unchanged from the MINLP-01 baseline
     (`"incumbent_feasibility; solver_trusted_oa"` /
     `"none"`, `canonical_verified = false`).
     On the canonical path `original_message` is replaced with a
     replay disclosure (incumbent verified, OA bound independently
     replayed) so the diagnostic text never contradicts `assurance`.
3. **Downgrade (mirrors `engine_milp`).** After certification, if
   `result.status ∈ {optimal, gap_satisfied}` and
   `!out.canonical_verified`, the status becomes `feasible` when
   `original_verified`, else `resource_limit`, with
   `message = "independent proof " + proof_status +
   "; optimality not certified"`. Consequences locked by this
   contract: without an accepted replay a MINLP solve never reports
   `Optimal` — including when proofs are disabled
   (`proof_status = "not_requested"`) — and proof exhaustion leaves
   the global result unverified. `infeasible` with an accepted replay
   stays `infeasible` with `canonical_verified = true`; `infeasible`
   without one stays `infeasible` but `verified = false`
   (`finalize` rule unchanged).
4. **Assurance label.** `derive_assurance` gains a branch before the
   solver-trusted guard: `proof_status == "accepted"` and tier ∈
   {`independent_oa`, `replayed_oa`} and `canonical_verified` ⇒
   `"oa_replayed"` (the label `numerical-policy.md` §3 reserves for
   exactly this condition; the doc is updated from "reserved" to
   "emitted under condition X"). The solver-trusted cap for
   `certificate_type` containing `solver_trusted_oa` is untouched, so
   every non-accepted path still lands at `original_primal_checked` or
   `unverified`, and `oa_replayed` never coexists with a small-gap
   shortcut.

## 6. Interfaces

1. **`minlp::MinlpSolution` capture (additive).** The OA loop stores,
   immediately after each master solve: `master_model`,
   `master_primal`, `master_status`, `master_objective`, and
   `master_history` (append-only; empty history ⇔ no master solved).
   These exist so the API layer can certify what the loop already
   proved without re-orchestrating the OA algorithm.
2. **`api::SolveResult` (additive).** `std::shared_ptr<const
   verify::OaProof> oa_proof`, `oa_proof_build_ms`,
   `oa_proof_verify_ms`; the generic `proof_*`, `guarantee_tier` and
   `proof_status` fields are reused unchanged.
3. **CLI.** `markov-cero-solve` emits `"oa_proof": "<text>"`,
   `"oa_proof_build_ms"`, `"oa_proof_verify_ms"` in its JSON next to
   `mip_proof`. New verifier tool
   `markov-cero-verify-minlp MODEL PROOF [--time-limit S]
   [--max-nodes N] [--max-values N] [--relative-gap T]`
   (`apps/markov_cero_verify_minlp.cpp`): parse the model with
   `io::parse_mps` (NLOBJ/NLCON sections), `read_oa_proof`,
   `verify_oa_proof`, print `VERIFIED:`/`REJECTED:`, exit 0/1, usage
   error 2 — the `markov-cero-verify-mip` pattern.
4. **Python.** `oa_proof` (string), `oa_proof_build_ms`,
   `oa_proof_verify_ms` exposed on the result object next to
   `mip_proof`; no new kwargs (the shared proof budgets are already
   exposed).
5. **Headers.** `oa_proof.hpp` is public; replay depends only on
   public minlp/qp/model headers plus `mip_proof.hpp`. The verifier's
   master assembly lives in `src/verify/oa_proof_internal.hpp` and is
   deliberately **not** the solve-side `minlp::build_master`: a defect
   in either assembly then fails closed (fingerprint mismatch ⇒
   rejected) instead of reproducing itself across the check
   (blueprint risk "verifier sharing too much code with generator").

## 7. Test obligations

Files: `tests/minlp02_proof_test.cpp`, `tests/minlp02_proof_attack_test.cpp`,
`tests/minlp02_enumeration_test.cpp` (each ≤300 lines), a CLI end-to-end
section in `scripts/repository_tools_test.py`, and a python binding
section in `python/tests/test_proof_guarantee.py`.

- **Positive fixtures.** Healthy case A through `api::solve_model`
  with defaults: `Optimal`, `assurance == "oa_replayed"`,
  `guarantee_tier == "independent_oa"`,
  `certificate_type` contains `independent_oa`, `oa_proof` attached,
  format version 1, fingerprint = recomputed source hash;
  write→read→`verify_oa_proof` round-trip accepts; the replayed
  `best_bound`/`relative_gap` are the published ones. Same for the
  infeasible-pair fixture (`kind == infeasible`, empty incumbent,
  `claims_infeasible`, stays `Infeasible`, `verified = true`) and for
  a maximize fixture (sense-mapped objective/bound/gap).
- **Downgrade and budgets.** Exhaustion (`mip_proof_max_nodes = 1`
  or a near-zero time limit) ⇒ `proof_status == "exhausted"`,
  status downgraded off `Optimal`, `canonical_verified == false`,
  message names the proof status; `enable_mip_proof = false` ⇒
  `not_requested` downgrade (§5.3, mirrored from MILP); a stripped
  fingerprint verifies at tier `replayed_oa` without canonical.
- **Reader rejections.** Version `2`, trailing data, truncated
  stream, tangent count over cap, malformed embedded MIP block,
  non-finite scalar — each rejected by `read_oa_proof`.
- **One mutation per proof field (attack file).** Control proof
  accepted first and last; then each of: stale fingerprint; missing
  tangent; altered gradient; altered point; altered rhs; negative
  weakening; out-of-range cut source index; duplicated cut;
  truncated embedded tree; forged claimed bound; forged incumbent
  coordinate (non-integral and source-infeasible); forged incumbent
  objective; flipped `source_sense` (maximize sign); forged
  convexity pivot; non-monotone forged history bound; kind/`claims_
  infeasible` inconsistency; NaN injection — every one `rejected`
  with a message naming the obligation. Seeded (fixed seed) fuzz:
  byte mutations restricted to fingerprint, version and count tokens
  never produce an accepted `independent_oa` result.
- **Random enumeration.** Seeded family of small instances (two
  integer variables on tiny domains, PSD quadratic objective, linear
  rows, bounded): exhaustive optimum by fixing each integer pair and
  solving the resulting convex QP exactly; the API solve with proofs
  enabled must return `Optimal`, an accepted replay, and an objective
  equal to the enumeration within §10 O1; a crafted infeasible member
  must return `Infeasible` with an accepted infeasible proof.
- **CLI/Python.** Checked-in fixture `tests/fixtures/minlp_case_a.mps`
  (NLOBJ + NLCON): solve → JSON carries `oa_proof`, assurance
  `oa_replayed`; `markov-cero-verify-minlp` accepts it (exit 0) and
  rejects `--max-nodes 1` exhaustion and a corrupted version (exit 1);
  the python result exposes the same proof string and one exhausted
  case stays off `Optimal`.
- **Regression.** Existing MINLP suites (library-level) are
  untouched; existing **API-level** MINLP expectations that asserted
  `Optimal` without a proof are updated to the §5.3 map — a semantic
  change locked by this contract, never a tolerance change.

## 8. Benchmark obligation

`scripts/bench_minlp02_proof.cpp` (target `minlp02_proof_benchmark`)
runs the frozen four-case stratum of `minlp-oa.md` §11 through
`api::solve_model` twice per case — shared proof budgets on (default)
and `enable_mip_proof = false` — and records per case: status,
assurance, proof status/tier, build ms, replay ms, serialized proof
bytes (`write_oa_proof`), objective/bound/gap; summary: accepted
fraction (expected 4/4), total bytes, and the disclosed on/off wall
difference (disclosure only — no speed claim). Written to
`evidence/minlp02-proof-2026-10-01.json` and listed in
`evidence/INDEX.md`; `scripts/check_json.py` must pass.

## 9. Documentation obligation

`numerical-policy.md` §3: `oa_replayed` changes from "reserved" to
"emitted iff proof accepted + canonical" (version bump if the doc
declares one). `docs/guides/VERIFY.md`: new section with the proof
version, the replay command, tier meanings, budget flags, and the
explicit limitation that floating-point replay is not a formal exact
proof. `evidence/INDEX.md` row per §8. `CHANGELOG.md` and
`docs/project/STATUS.md` entries on completion.

## 10. Numeric allowances (locked by this contract)

| # | Quantity | Tolerance | Source |
|---|---|---|---|
| P1 | convexity pivot cross-check abs/rel | `1e-6 · max(1, \|pivot\|)` | evidence tamper check only |
| P2 | PSD assessment | `1e-10` | `qp::assess_convexity` default |
| B1 | claimed bound vs replayed `L*` | `1e-6 · max(1, \|L*\|)` | two different exact-tolerant solvers |
| B2 | `L* ≤ U` allowance | `feasibility_tolerance · max(1, \|U\|)` with `feasibility_tolerance = 1e-6` | `minlp-oa.md` §7.6 rule |
| I1 | integrality | `1e-6` | `minlp-oa.md` §6 |
| F1 | nonlinear feasibility | `1e-6` (verifier options default) | `minlp-oa.md` §6 |
| O1 | objective re-evaluation vs claim | `1e-6 · max(1, \|claim\|)` | `minlp-oa.md` §6 |
| G1 | gap cross-check slack | `1e-9` absolute over the locked `1e-3` | replay vs record |

Cut replay (`1e-8`) and outward weakening (`1e-9`) stay
`minlp-oa.md` §12's values via `kOaCutReplayTolerance` /
`kOaCutWeakening`; this contract introduces no other guards.

## 11. Limits, change procedure, do-not-do

1. **Caps.** Time: shared proof deadline (default 5 s, clipped by the
   solve deadline). Size: `maximum_tangents = 10000` (= default
   `max_oa_cuts`), `maximum_nodes = 10000`,
   `maximum_witness_values = 4e6`, fingerprint 4096 bytes, plus the
   pre-build memory charge of §4.2. Model: the inherited 512-variable
   screening cap. Reader and verifier poll the deadline (parse
   counts against the budget).
2. **Versioning.** `kOaProofFormatVersion` is checked by strict
   equality in writer, reader and verifier, and pinned by tests;
   bumping it updates all three plus the python/CLI fixtures in the
   same change. The embedded MIP block keeps `kMipProofFormatVersion`
   untouched.
3. **Do not.** Do not set `canonical_verified = true` from any path
   except an accepted `verify_oa_proof`; do not call `build_mip_proof`
   (or any solver) from the replay path; do not reuse the solve-side
   `build_master` inside the verifier; do not emit `oa_replayed` for
   `replayed_oa`, exhausted or rejected outcomes; do not report
   `Optimal` when §5.3's downgrade applies; do not extend the class
   (callbacks, nonconvex, >512 vars) through the proof layer; do not
   add new assurance labels without a `numerical-policy` amendment.
4. **Change procedure.** Edits to obligations or tolerances require a
   contract version bump and a re-run of §7's full suite; evidence
   files are immutable — a changed benchmark writes a new dated file.
