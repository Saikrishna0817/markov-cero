# Benchmark campaigns

How to declare, run and read a benchmark campaign. The binding rules are
in the [benchmark campaign contract](../contracts/benchmark-campaign.md);
this guide is the operator's view of the same rules.

## Three commands, in this order

```bash
python3 scripts/run_bench01_campaign.py preregister \
  --campaign-id bench01-<date> --binary <solver> --workers 4 \
  --out evidence/bench01-preregistration-<date>.json

python3 scripts/run_bench01_campaign.py run \
  --prereg evidence/bench01-preregistration-<date>.json \
  --csv evidence/bench01-campaign-<date>.csv

python3 scripts/run_bench01_campaign.py summarise \
  --prereg evidence/bench01-preregistration-<date>.json \
  --csv evidence/bench01-campaign-<date>.csv \
  --out evidence/bench01-campaign-<date>.json
```

`preregister` refuses to overwrite an existing declaration, so a
different subset, cap, thread count or repeat count always means a new
file. `run` refuses to start when the binary no longer matches the
declared sha256, and refuses to resume over rows whose solver hash, cap,
threads or manifest date disagree. `summarise` is pure: it recomputes
every metric from the raw rows and echoes the preregistration by sha256.

## Artifacts

| Artifact | Contents |
|---|---|
| `evidence/bench01-preregistration-<date>.json` | Declared subset, order, caps, threads, repeats, workers, metric definitions, tolerances, reference optima and limitations. Written before the first cell runs. |
| `evidence/bench01-campaign-<date>.csv` | One row per declared cell × repeat, in the contract's fixed 29-column order. The raw record; rows are never deleted. |
| `evidence/bench01-campaign-<date>.json` | Preregistration echo by sha256, host and build record, raw-CSV sha256, the §6 summary with numerators and denominators, and the limitations. |

The CSV is the raw log: each row carries `parent_wall_ms` (the
`process_wall` scope), the child's own `solver_runtime_ms`,
`peak_rss_kib`, status, objective, bound, gap, node and iteration
counters, the reference optimum where the harness holds one, and the
independent feasibility verdict. The driver's stdout is progress only
and is not evidence.

## First recorded campaign — 2026-10-01

`evidence/bench01-preregistration-2026-10-01.json` (written first),
`evidence/bench01-campaign-2026-10-01.csv` (1,325 raw rows),
`evidence/bench01-campaign-2026-10-01.log` (driver progress) and
`evidence/bench01-campaign-2026-10-01.json` (the §6 summary) are that
campaign's record. It completed with 0 harness exceptions and 0 parent
timeouts: 270 `ok` rows over 54 instances, 1,010 `absent` and 45
`hash_mismatch`. Headline numbers — 31/265 solved (0.117), 0/54 cells
disagreeing on status or objective across repeats, 165/170 independent
primal re-checks passing — and the gaps it exposed are read in the
[evidence index](../../evidence/INDEX.md) and the
[status record](../project/STATUS.md). One of those gaps, the
`InvalidModel` rejection of `data/netlib/e226.mps` and `grow7.mps`,
was closed afterwards by the [MPS rim contract](../contracts/mps-input.md);
those two cells still carry their 2026-10-01 `InvalidModel` rows,
because a recorded campaign row is never rewritten — they can only be
re-run as part of a new preregistration. It is one host, one binary and
no second solver, so it carries no speed claim.

## What this checkout declares

`evidence/frozen-instances-20260928.json` fixes the corpus:

| | Declared | Present here |
|---|---|---|
| All suite instances | 265 | 63 |
| LP | 98 | 31 |
| MILP | 149 | 28 |
| QP | 18 | 4 |
| Split holdout / tune / train | 38 / 47 / 180 | 13 / 14 / 36 |
| With a reference optimum | — | 27 |

The 202 absent instances keep their rows as `absent` and stay in every
denominator. Present instances whose file hash disagrees with the frozen
manifest are `hash_mismatch`: they are not executed, and they too stay in
the denominator — 63 present minus 9 mismatches leaves **54 executable
cells** on this checkout.

All nine mismatches are the curated `data/cases/*` files
`capitanescu_dc_opf`, `li_crude_blending`, `neiro_refinery_scheduling`,
`pochet_lot_sizing`, `process_network_large`, `production_planning_large`,
`refinery_scheduling_large`, `shapiro_network_flow` and
`supply_chain_large`. Commit `a9c7e42` (2026-09-29) added `*` comment
headers to exactly those nine files the day after the 2026-09-28 freeze,
deleting nothing — the model semantics are unchanged, but the bytes no
longer match the frozen record and the contract does not let the harness
guess. Re-freezing would mean a new manifest and a new preregistration,
never an edit of `frozen-instances-20260928.json`.

## Host, toolchain and timing scope

The machine and toolchain are recorded in
[`evidence/hardware.md`](../../evidence/hardware.md) and repeated in the
`host` block of the campaign summary JSON (platform, machine, processor,
Python version, CPU count, solver path and solver sha256).

Timing is `process_wall`: the parent measures wall clock from spawn to
reap, including model parse, CLI start-up and result serialisation. The
child's `runtime_ms` is recorded alongside and never substituted for it.
A child that outlives `cap + parent_slack_s` is `parent_timeout`, which
is a real row, never a dropped one.

Peak RSS is sampled from `/proc/<pid>/status` `VmHWM` every 2 ms while
the child runs. `wait4`'s `ru_maxrss` is **not** used: on this host it
reports `max(parent_high_water, child_high_water)`, so a parent that has
ever held more memory than its children poisons every reading, and
`/usr/bin/time` is not installed. The 2 ms grid means a child that
finishes inside one interval can be recorded before its true peak —
every such reading is a lower bound, and the 2026-10-01 run contains
five QP rows below 3,000 KiB for exactly that reason.

## Caps, threads, repeats, workers

| Knob | Value | Source |
|---|---|---|
| LP / QP cap | 60 s | `frozen-instances-20260928.json` → `timing.class_time_limits_s` |
| MILP cap | 300 s | same |
| Threads | 1 for every child | `matched_threads` |
| Repeats per cell | 5, index 0 cold, 1–4 warm | `min_repeats` |
| Workers | 4 concurrent single-threaded children | the preregistration |

Repeats run in rounds: every cell's cold observation completes before
any warm observation begins, so a cell never runs two repeats at once.
Cold and warm are summarised separately and never averaged together.

## Reading a result

`run_state` describes the process; `status` is whatever the solver
returned and is never rewritten:

| `run_state` | Meaning |
|---|---|
| `ok` | solver JSON parsed, a status was present |
| `solver_error` | no usable solver JSON — `status` recorded `ParseError` |
| `parent_timeout` | parent watchdog fired |
| `absent` | model not in this checkout |
| `hash_mismatch` | model sha256 disagrees with the frozen manifest |
| `thread_mismatch` | ran with a non-preregistered thread count |
| `binary_mutated` | the solver binary changed during the campaign |

A non-zero exit code is not an error: `apps/json_output.hpp:16` maps a
status to `0..7`, so an exit of `5` with a parseable `ResourceLimit` JSON
is an `ok` row with `notes` recording the code.

Every metric in the summary carries its numerator, its denominator and,
when it drops rows, the run states it dropped:

- `solved_fraction` — declared cells whose every `ok` row is `Optimal`,
  over all declared cells. Cells with no `ok` row are unsolved, not
  excluded.
- `objective_disagreement` — cells whose warm objectives spread beyond
  T4 (`1e-6 · max(1, |obj|)`), over cells with ≥ 2 warm objectives.
- `reference_agreement` — `ok` rows differing from the harness's
  reference optimum beyond T1/T2/T3, per class, over `ok` rows whose
  instance holds a reference.
- `independent_primal_failures` — `ok` rows whose harness-recomputed
  feasibility exceeds T5/T6/T7 (`1e-6` absolute), over `ok` rows the
  harness could check. `not_checked` rows are named, never counted as
  passes.
- `time_distribution` and `rss_distribution` — n, min, median, max, per
  class, cold and warm as separate keys.

The independent check is a second implementation: `scripts/support/bench01_mps.py`
parses the original model file itself and recomputes row activities,
bound residuals and integrality from the returned `primal`. It never
reads the solver's `row_activities`, `maximum_primal_violation` or
`verified` field. The harness bar is absolute and class-independent, so
it can be stricter than a solver-side gate — the QP engine gate is
`1e-4` (`convex-qp.md` §2.4) — and such a row is still
`independent_primal_failure` with the raw violation retained.

`reference_agreement` is a check of *objective proximity*, not of
optimality claims: its denominator is every `ok` row whose instance
holds a reference, including `ResourceLimit` and `NumericalFailure`
rows whose incumbent was never close to the optimum. Read it alongside
the status breakdown — in the 2026-10-01 run all 60 MILP "disagreements"
are such rows, and 0/60 LP plus 0/10 MILP `Optimal` rows disagree.

Rows whose solver output carries no `primal` (a MILP run that found no
incumbent, or a failure before a witness existed) are `not_checked`,
never counted as passes and never as failures.

## Statistical method

Central tendency is the median. Cold and warm are reported as separate
series. Ratios between two solvers would be reported as geometric means
over paired cells, and only from runs with the same binary, thread
count, cap and repeat structure. This campaign publishes no speed ratio
and no significance test, therefore no significance claim.

## Known limitations

1. **Single host.** No second-host timing exists, so the promotion and
   second-host acceptance criterion of the release contract is
   documented as NOT MET.
2. **63 of 265.** The campaign covers only the instances present in
   this checkout. Presenting it as coverage of the 265-instance frozen
   corpus, or of any other host, is prohibited by §12.4.
3. **Worker interference.** Four concurrent single-threaded children
   share one machine, so absolute wall times include interference.
   Correctness metrics are unaffected; no sequential-versus-concurrent
   speed comparison is published.
4. **Reference coverage.** 27 of 63 present instances carry a reference
   optimum, so `reference_agreement` covers fewer rows than
   `solved_fraction`.
5. **Nine hash mismatches.** The post-freeze comment edit described
   above removes nine present instances from execution while leaving
   them in the denominator, so 54 of 63 present cells actually run.
6. **Sampled RSS.** A peak read on the 2 ms grid is a lower bound for
   any child shorter than one interval.
