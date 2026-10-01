# Benchmark campaign contract — v1

**Contract version:** 1 (2026-10-01, blueprint BENCH-01).
**Status:** binding for every benchmark/differential campaign run under this
repository after this revision, and for the evidence it produces.
**Scope:** the preregistration, run protocol, row retention, independent
checks, metrics, output schema and self-test of `scripts/run_bench01_campaign.py`
and its support modules. It does **not** cover the per-stratum contract
benchmarks already fixed by `convex-qp.md` §6, `milp-node-bounds.md` §7.6,
`miqp-node-bounds.md` §7.8, `nlp-local-sqp.md` §7, `nlp-restoration.md` §8,
`minlp-oa.md` §11 and `minlp-proof-replay.md` §8; those records stay separate
and are never merged into a campaign summary.

Rules this contract exists to enforce (blueprint §29 and BENCH-01):

1. The denominator is fixed **before** the first run and every declared cell
   keeps its row — absent, unsupported, timed-out, crashed and
   numerically-failed cells included.
2. A harness result is a measurement, never a fabrication: no fabricated
   row may be reported as a benchmark outcome, and the harness's own
   fabricated records exist only inside its self-test.
3. Primals are checked by a second implementation against the original
   model file, not by re-reading the solver's own verification fields.
4. Speed is only ever reported from matched runs: same binary, same
   thread count, same cap, same repeat structure.
5. Nothing in this contract upgrades a solver status. Every row reports
   the status the solver returned.

---

## 1. What a campaign may claim

A campaign run may claim, for the declared subset and the recorded
binary/host:

- the **solved fraction** — declared cells whose every `ok` row reports
  `Optimal` — over the full declared denominator;
- **status and objective agreement/disagreement** between repeats and
  against reference optima where the harness holds one;
- an **independent feasibility verdict** for returned primals, computed by
  the harness's own model reader;
- a **paired process-wall time distribution** and a **peak-RSS
  distribution** for the rows that produced a result;
- node counts, gaps and iteration counters as measured.

It may not claim: breadth beyond the declared subset, correctness on the
202 frozen-manifest instances absent from this checkout, cross-host
reproducibility, GPU/ML/NLP/MINLP production support, or any speed
superiority over another solver. Comparative speed claims require a
separate preregistration naming the comparator binary and its version.

---

## 2. Preregistration

`evidence/bench01-preregistration-<date>.json` is written **before** any
cell of that campaign executes and is then immutable. It records:

| Key | Meaning |
|---|---|
| `campaign_id` | stable identifier, e.g. `bench01-2026-10-01` |
| `written_before_any_run` | must be `true` |
| `manifest` | path + sha256 of the frozen instance manifest |
| `declared_subset` | every declared cell: id, suite, class, split, sha256, `present` |
| `absent` | declared cells not present locally, kept in the denominator as `absent` |
| `caps` | per-class solver wall caps in seconds |
| `threads` | matched thread count for every process |
| `repeats` | `per_cell`, and which index is the cold observation |
| `workers` | concurrent single-threaded processes, and the interference caveat |
| `metrics` | the metric definitions of §6, quoted, not paraphrased |
| `tolerances` | the objective and feasibility tolerances of §7 |
| `known_limitations` | at minimum: single host, absent instances, worker interference |

Re-running a campaign with a different subset, cap, thread count or repeat
count is a **new preregistration**, never an edit of an old one.

## 3. Frozen inputs

The campaign binds:

- `evidence/frozen-instances-20260928.json` (or its successor) — the
  subset, family, problem class and seeded split of every instance, plus
  each instance's sha256;
- `scripts/support/frozen_timing_config.py` — the cap vocabulary and
  `MIN_REPEATS`;
- the solver binary's sha256, recorded once at start and re-checked at
  end; a mid-run binary change invalidates the campaign (row
  `binary_mutated`), it never silently continues.

Model files are read from the paths the manifest names. A model whose
sha256 does not match the manifest is reported `hash_mismatch` and kept
in the denominator.

## 4. Run protocol

1. **Matched processes.** One solver process per (instance, repeat). The
   parent observes process wall clock around the child, from spawn to
   exit, including model parse and result serialisation — this is the
   `process_wall` scope of `frozen_timing_config.py`. The child's own
   `runtime_ms`/`total_ms` are recorded alongside but never substituted
   for it.
2. **Threads.** Every child runs with the same `--threads` value
   (`threads = 1` unless a preregistration says otherwise). A cell that
   needed a different thread count is recorded `thread_mismatch` and
   excluded from timing aggregates, not from the denominator.
3. **Repeats.** `repeats.per_cell = 5`: index 0 is the **cold**
   observation, indices 1–4 are **warm**. Cold and warm are summarised
   separately and never averaged together; the cell statistic for any
   reported central tendency is the warm median.
4. **Order.** Instances are processed in frozen-manifest split rank
   (holdout, tune, train) then id ascending. The order is fixed by the
   preregistration, not by observed difficulty. Repeats run in **rounds**:
   the whole of index 0 completes before index 1 starts, and so on, so a
   cell never runs two of its own repeats concurrently and a warm
   observation can never overlap its cell's cold one. Within a round the
   worker pool shares cells; across rounds there is a barrier.
5. **Caps.** Per-class solver `--time-limit` from the preregistration.
   The parent additionally enforces `cap + parent_slack_s` and records a
   child that exceeded it as `parent_timeout` — a real row, never a
   dropped one.
6. **Concurrency.** `workers > 1` runs single-threaded children
   concurrently. It is allowed only if the preregistration declares the
   worker count and states that absolute wall times then include
   interference; correctness metrics are unaffected, speed comparisons
   against a sequential run are not published from such a campaign.
7. **Resume.** A resumed campaign refuses rows whose recorded solver
   sha256, cap, thread count or repeat index differ from the
   preregistration.

## 5. Row retention and the denominator

Every declared cell × repeat emits exactly one row with a `run_state`:

| `run_state` | Meaning |
|---|---|
| `ok` | process exited, solver JSON parsed, a `status` was present |
| `solver_error` | the process emitted no usable solver JSON — stdout missing, invalid JSON, or no `status` field (kept with `status=ParseError`) |
| `parent_timeout` | parent watchdog fired at `cap + parent_slack_s` |
| `absent` | the model file is not present in this checkout |
| `hash_mismatch` | model sha256 disagrees with the manifest |
| `thread_mismatch` | the child ran with a non-preregistered thread count |
| `binary_mutated` | the solver binary changed during the campaign |

`solver_error` is decided by the **payload**, never by the exit code.
`apps/json_output.hpp:16` maps a status to `0..7`, and this CLI's
contract is "a zero exit code means a verified optimal solve; inspect
the JSON at a resource or numerical limit" (`QUICKSTART.md`). A child
that exits `5` with a parseable `ResourceLimit` JSON is therefore an
`ok` row: `run_state=ok`, `status=ResourceLimit`, and the non-zero code
is kept in `notes`. Only a child whose stdout cannot be read as a solver
result is `solver_error`.

`solved_rows / declared_cells` is the solved fraction and its
denominator is the **declared** cell count, not the `ok` row count.
No row may be deleted, retried into existence, or summarised away; a
retry produces an additional row that is superseded and marked
`superseded_by`, and both remain in the raw file.

## 6. Metrics

Computed from the raw rows, all of them reported with their numerator
and denominator:

| Metric | Definition |
|---|---|
| `solved_fraction` | declared cells whose every `ok` row reports `Optimal` ÷ declared cells; a cell with no `ok` row is unsolved, never excluded |
| `status_counts` | count per status over all rows |
| `objective_disagreement` | cells whose warm-median objective differs from another repeat of the same cell by more than §7's tolerance |
| `reference_agreement` | `ok` rows whose objective differs from the harness's reference optimum by more than §7's tolerance, per class |
| `independent_primal_failures` | `ok` rows whose harness-recomputed feasibility exceeds §7's tolerance |
| `time_distribution` | cold and warm process-wall per class: n, min, median, max, and the paired per-instance ratios when a second solver is present |
| `rss_distribution` | per-child peak RSS (KiB) sampled from `/proc/<pid>/status` `VmHWM` while the child runs, per class: n, min, median, max |

`wait4`'s `ru_maxrss` is **not** used: on this host a child inherits its
parent's high-water mark across `fork`/`vfork` and `exec` does not reset
it, so `ru_maxrss` reports `max(parent_high_water, child_high_water)` and
a parent that has ever held more memory poisons every reading. `/usr/bin/time`
is not installed here. `VmHWM` is per-mm and resets at exec; it is
sampled every 2 ms for the whole life of the child, and the maximum over
those samples is the row's `peak_rss_kib`. A child so short that no
sample lands after exec is reported with the samples it did produce.
| `nodes_and_gaps` | `nodes_explored` and `relative_gap` counts as measured, only over rows that reported them |

No metric is computed over a silently reduced population: if a metric
excludes rows, the excluded `run_state` values are named next to it.

*Clarification recorded 2026-10-01, before the first campaign of this
contract completed.* The `solved_fraction` row above was originally
worded "rows with status `Optimal` ÷ declared cells", which is
unit-inconsistent (a repeat count in the numerator, cells in the
denominator) and admits a value above 1. The denominator — declared
cells — and every other metric row were unchanged, the harness has
always computed the cell-level quantity defined above, and
`tests/bench01_harness_selftest.py` binds it (a fabricated four-cell
set yields numerator 2, denominator 4+1). The preregistration
`evidence/bench01-preregistration-2026-10-01.json` quotes the original
shorthand of this row in its `metrics` block and is not edited; the
quantity it binds is the one defined here. No subset, cap, thread,
repeat, worker, tolerance or limit changed with this clarification, so
§12.5 is untouched.

## 7. Tolerances (locked)

| # | Quantity | Tolerance |
|---|---|---|
| T1 | LP objective vs reference | `1e-5 · max(1, \|ref\|)` |
| T2 | MILP objective vs reference | `1e-4 · max(1, \|ref\|)` |
| T3 | QP objective vs reference | `1e-4 · max(1, \|ref\|)` |
| T4 | cross-repeat objective (same cell) | `1e-6 · max(1, \|obj\|)` |
| T5 | harness row feasibility | `1e-6` absolute |
| T6 | harness bound feasibility | `1e-6` absolute |
| T7 | integrality | `1e-6` absolute |

T5-T7 are the harness's own locked absolute bars. They are
class-independent and may be **stricter** than the acceptance gate a
solver-side verifier uses for a given class — the QP engine gate is
`1e-4` (`convex-qp.md` §2.4) and `verify::Tolerance` defaults to
`1e-7 + 1e-7 · scale` (`numerical-policy.md` §5.11). A row whose
harness-recomputed violation exceeds T5/T6/T7 while staying inside the
solver's own class gate is still `independent_primal_failure`: the raw
value is retained in `independent_max_violation`, the solver's status and
its `verified`/`assurance` fields are recorded exactly as returned, and
`notes` names the disagreement. Neither direction is rounded away.

A row failing T5/T6/T7 is `independent_primal_failure`; the row stays,
and the harness never rewrites the solver's status.

## 8. Independent primal check

The harness re-derives feasibility with **its own** reader:
`scripts/support/bench01_mps.py` parses the original model file
(ROWS/COLUMNS/RHS/RANGES/BOUNDS/ENDATA with `INTORG`/`INTEND` markers in
either spelling the frozen corpus uses) and recomputes row activities,
bound residuals and integrality from the `primal` vector in the result
JSON. It never reads the solver's `row_activities`,
`maximum_primal_violation` or `verified` field to make its verdict, and
it never imports a solver-side parser.

The reader follows the MPS standard where the frozen corpus deviates:
repeated `(row, column)` entries accumulate, an RHS/RANGES line with an
even token count omits the vector-name field, and a quadratic objective
section is accepted and ignored because it cannot change a row
activity. Variables are matched by name when the result carries
`variable_names`, and by file order only when it does not.

A model containing a section this reader does not implement is reported
`not_checked` with the section name, kept in the denominator and counted
separately in `independent_primal_failures`'s exclusion note. An
unparsable model is `not_checked`, never a pass. A result whose `primal`
is empty, absent, or whose length disagrees with `variable_names` or
with the parsed model is `not_checked` with that reason — the harness
does not guess which vector the solver meant.

## 9. Output schema

Two artefacts, both written under `evidence/`:

1. `bench01-campaign-<date>.csv` — one row per cell × repeat, columns in
   fixed order:
   `campaign_id,manifest_version,solver,solver_sha256,instance,suite,problem_class,split,repeat,cold_warm,run_state,threads,cap_s,parent_wall_ms,solver_runtime_ms,status,verified,assurance,objective,best_bound,relative_gap,nodes,lp_iterations,peak_rss_kib,reference_objective,objective_delta,independent_check,independent_max_violation,notes`
2. `bench01-campaign-<date>.json` — the preregistration echo, host and
   build record, the §6 summary with numerators/denominators, and the
   path of the raw CSV.

`manifest_version` is the frozen manifest's `date` field; `campaign_id`
ties the CSV to the JSON. Both files are written even when the campaign
is incomplete, with `complete: false` and the reason.

## 10. Harness self-test

`tests/bench01_harness_selftest.py` runs in the default CTest set (no
external solver, no benchmark data required) and must exercise the
harness's classification and summarisation with **fabricated** records:

- a `parent_timeout` row and a `solver_error`/`ParseError` row stay in
  the denominator and never become `Optimal`;
- an `objective_disagreement` between two fabricated repeats is detected
  at T4 and reported with its cell;
- a fabricated independent-check failure at T5 is reported, and the
  solver status in that row is not modified;
- an `absent` cell stays in the denominator;
- the cold/warm split is reported separately and never merged;
- `solved_fraction` uses the declared denominator, not the `ok` count.

Fabricated records live only in this self-test. No fabricated record may
be written into `evidence/`.

## 11. Documentation obligation

Every campaign publishes, next to its artefacts: host and toolchain
(`evidence/hardware.md` reference), model collections and counts,
caps/threads/repeats, raw-log locations, the statistical method (median
and geometric-mean ratios, no significance claim without a stated test),
and the limitations — absent instances, single host, worker
interference, and the float-replay caveat where a proof was involved.
`evidence/INDEX.md`, `docs/project/STATUS.md` and `CHANGELOG.md` are
updated in the same change as the artefacts.

## 12. Do not do

1. Do not drop, re-run-into-existence, or reclassify a failed row.
2. Do not compare a local NLP objective to a claimed global optimum and
   call it agreement.
3. Do not report a speed ratio from runs with different thread counts,
   caps, binaries or repeat structures.
4. Do not present a campaign on the 63 present instances as coverage of
   the 265-instance frozen corpus, or of any second host.
5. Do not write a preregistration after observing results for that
   campaign.
