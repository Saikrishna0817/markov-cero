# Sparse LP path contract (v1)

Binding contract for how `api::solve` represents and dispatches linear
programs. It closes blueprint task LP-01: the LP path canonicalizes to a
sparse CSC model and must never materialize an uncapped dense `m×n` canonical
matrix on its way to an engine.

Related contracts and evidence:

- [Resource limits contract](resource-limits.md) — stop attribution for the
  dimension envelope below (`work_limit`).
- [Numerical contract](numerical-policy.md) — tolerances under which the
  differential results below are compared.
- `evidence/lp-sparse-rss-*.json` — measured peak RSS, wall time and
  iterations for the dense-adapter vs sparse entries (benchmark record).

## 1. Sparse-first rule

1. **Canonicalize sparse.** The LP path builds exactly one working model per
   solve: `transform::sparse_canonicalize` produces a
   `SparseCanonicalModel` (CSC, rhs, objective, canonicalization record).
2. **Dispatch without densifying.** Primal, dual and IPM dispatch in
   `src/api/engine_lp.cpp` all receive the sparse working model directly.
   The path contains no `to_dense()` call and no `dense_conversion_bytes`
   charge; the `dense_convert` stage is retired.
3. **Engines expose sparse entries as primaries.** `lp::reference::solve`,
   `lp::dual::solve`, `lp::dual::Session::resolve`,
   `lp::dual::make_basis_state`, `lp::dual::fingerprint` and
   `lp::interior::solve` take `SparseCanonicalModel`. The dense
   `CanonicalModel` overloads that remain are **explicit adapters**: they
   convert through `transform::sparse_from_dense` (skip structural zeros,
   column-major, row-sorted) and then execute the identical sparse code.
   Dense-overload and sparse-overload calls on equivalent content run the
   same arithmetic in the same order.
4. **Witnesses stay sparse.** Every accepted LP result is re-checked by
   `verify::verify_sparse_result` against the sparse canonical model
   (`src/api/engine_lp.cpp`); the dense verifier wrapper converts first.

## 2. What "no uncapped dense allocation" means

- The public LP path allocates `O(nnz + m + n)` — CSC buffers, work vectors
  and basis factors — never `O(m·n)` for the canonical matrix.
- The largest dense buffers the LP path may still allocate are per-iteration
  vectors of length `m` or `n`, which is inherent to revised simplex.
- Compatibility paths outside the public LP dispatch (MIP node LP small-basis
  solve, strong branching child LPs, MILP primal repair/pump heuristics,
  PDLP's dense crossover canonicalization) keep their own named byte or
  dimension caps and are out of scope here; each must stay behind an explicit
  cap, never unbounded.

## 3. Supported size envelope and the dense adapter cap

The LP solve envelope is an explicit, named dimension check performed at
dispatch in `src/api/engine_lp.cpp`, before any engine runs:

| Bound | Value | On violation |
|---|---|---|
| canonical rows | 4096 | `std::length_error` escapes the API boundary |
| canonical columns | 16384 | `std::length_error` escapes the API boundary |

On violation the boundary applies the resource contract mapping
(`resource-limits.md` §3 R3): `status == resource_limit`,
`stop_reason == work_limit`, `failure_site == memory_or_factor_limit`,
never `verified`. The boundary test is
`tests/stop_reason_test.cpp::test_dimension_limit_is_work_limit`
(CTest `stop_reason_boundary`).

Notes:

- The envelope matches the engines' internal dimension caps
  (`lp::reference` `maximum_rows`/`maximum_columns`, `lp::dual`
  `maximum_rows`/`maximum_columns`); the named check exists so the limit is
  reported as a `work_limit` stop instead of an engine-internal status with
  no attribution.
- The check runs after presolve and scaling, so a model whose post-presolve
  size fits the envelope still solves — identical boundary semantics to the
  retired dense-conversion check.
- `transform::SparseCanonicalModel::to_dense()` keeps its own named cap
  (rows ≤ 4096, columns ≤ 16384, else `std::length_error`) for the
  compatibility paths that still convert. It is no longer on the public LP
  dispatch.

## 4. Warm-start basis fingerprints

- A basis fingerprint is defined **once**, over the sparse canonical content:
  dimensions, the column-major stream of `(row index, double bits)` for every
  nonzero, and the objective coefficients
  (`src/lp/dual/dual_simplex_select_entering_column.cpp`).
  Dense adapters convert to CSC first, so both entries compute the same
  fingerprint for equivalent content.
- `BasisState` files written before this contract (fingerprints hashed over
  the dense value array) no longer match; the warm start is rejected by
  metadata validation and the dual engine falls back to a cold solve with
  `used_cold_fallback == true`. This is a stale-input outcome, not an error.
- Interchange guarantee: a `BasisState` created from one entry validates
  against the other entry **iff** both saw identical canonical content.
  Content canonicalized by different canonicalizers (dense vs sparse) is not
  guaranteed identical to the last bit; disagreement is reported as a stale
  fingerprint and a cold fallback, never as a wrong answer.
- A cold solve delegated to the reference oracle can carry an out-of-range
  `solution.basis`; the dual engine's cold path drops that warm start instead
  of publishing it (`basis_state` empty). Interchange applies to whichever
  side published a state — callers must exchange `Result::basis_state`, not
  raw `solution.basis`.

## 5. Differential obligation

Before the dense engine entry may be considered retired from dispatch, the
dense-adapter and sparse entries must be differential-tested on the frozen
Netlib set (`data/compare/netlib.txt`) plus randomly permuted equivalent
models (`tests/lp_sparse_differential_test.cpp`):

1. accepted statuses agree exactly;
2. objectives agree within the numerical contract's declared tolerances;
3. both witnesses pass `verify_sparse_result`;
4. warm-start basis interchange across the two entries succeeds when content
   is identical.

Benchmark obligation: matched peak RSS, wall time, iterations and factor
nonzeros for both entries on the frozen set, recorded in `evidence/` and
indexed from `evidence/INDEX.md`.

Executed 2026-09-30:

- Differential: all 17 frozen instances — statuses, objectives, both
  witness boundaries and warm-start interchange agree across entries.
  `blend`, `scsd1` and `scsd6` fail simplex phase I *identically on both
  paths* and still receive the agreement, fingerprint and row-permutation
  checks.
- LP-01 card triage (`bore3d`, `scsd1`, `scsd6`, `blend`): each fails
  end-to-end with `NumericalFailure` at `engine=primal` — simplex phase I
  fails, IPM finds the optimum, its crossover candidate is rejected, and
  the final witness check does not certify the interior point. Re-running
  the same models on the pre-LP-01 revision (stash of this change, clean
  rebuild) produced the **same statuses and objectives**, so this is
  pre-existing at HEAD, not introduced by the sparse path; the entries
  still agree exactly on every one of them (dense/sparse fingerprint
  match included). Tracked as a deferred defect; LP-01 makes no claim
  about these four models beyond agreement. Note the reproduction path:
  `bore3d` and `scsd1` fail through the CLI as well, while `blend` fails
  only through the library differential harness (no presolve/scale) and
  solves `Optimal` through the CLI — see the GAP-01 context note below.
- GAP-01 addition (2026-10-01), `etamacro`: outside the LP-01 triage and
  outside the 17-fixture differential set, so it has no entry-agreement
  record. All four CLI engines (`primal`, `dual`, `ipm`, `pdlp`) return
  byte-identical status, objective, iteration count and message on HEAD
  **and on the pre-LP-01 revision `32be1ec`** — `NumericalFailure`
  (primal/ipm: the same `cok_fail ook_fail` witness rejection at
  objective −755.714309; dual: phase I; pdlp: 100 000-iteration limit) —
  so the failure is pre-existing, not a sparse-path regression
  ([record](../../evidence/gap01-etamacro-2026-10-01.json)). The cause
  is numerical, not dispatch: canonical complementarity 2.15e-3 against
  a tolerance three orders tighter, on a model with estimated condition
  8.3e17. The witness check refusing to certify is the contract working;
  no fix and no objective claim is made. No dense/sparse differential
  was run for this model, and adding one would mean extending
  `data/compare/netlib.txt`, which is frozen after results exist.
  Follow-up 2026-10-03: `etamacro` now solves `Optimal` + verified
  (objective −755.715233375, message
  `reference primal revised simplex optimum`) after the deferred-defect
  closure recorded below; the 2026-10-01 record above stands as measured
  on that date.
- Executed 2026-10-03 — deferred defect **closed**
  ([record](../../evidence/lp-deferred-fix-2026-10-03.json)). The four
  triage models and `etamacro` now solve end-to-end through the CLI, and
  all 17 frozen instances pass the differential with full interchange
  (30 checks, ≈9.6 s). Three root causes in the reference primal path,
  in measured order of impact: (a) the adaptive Bland trigger fired at
  20 consecutive degenerate steps, so `scsd1` phase II wandered 230 085
  threshold-Bland pivots where pure Dantzig finishes in 1023 (longest
  consecutive frozen run 783; every other frozen-set model stays under
  124) — the threshold is now `kAntiCyclingDegenerateSteps = 5000`,
  above every measured run and below the default 10 000-pivot budget,
  so a genuine cycle still reaches Bland repair inside a single solve;
  (b) `Options::bland_anti_cycling` defaulted to `true`, which the dual
  engine's cold fallback inherits from the reference default (it does
  not copy the flag) — it now defaults to `false`, with the
  deterministic Bland retry-on-numerical-failure path still setting it
  explicitly; (c) an is-cycling pair-reversal guard parked columns via
  `candidate_tried` without `rejected_column`, so the revival scan
  skipped them and row-permuted `scsd1` reported a false optimum
  (`dok_fail`, rc −0.199 on the parked column) — the guard is deleted
  (a cycle is degenerate and bounded by the 5000-step threshold) and
  the revival scan now also revives step-local parks
  (`(!rejected_column[j] && !candidate_tried[j]) || basic[j]`). The
  seven CLI cells (`bore3d`, `scsd1`, `scsd6`, `blend`, `etamacro`,
  `e226`, `grow7`) report `Optimal` + `verified` with the message
  `reference primal revised simplex optimum` — a direct reference
  solve, no IPM fallback; `bore3d`'s 1373.080394208 is cross-confirmed
  by the dual engine with both witnesses verified. Suite 122/122,
  `check_source_limits` 0 violations (the oversized files were split
  in the same change: `engine_lp_ipm.cpp`, `ipm_crossover.cpp`,
  `revised_simplex_pivot.cpp`). A recorded BENCH-03 row is never
  rewritten; BENCH-04 re-measured the same seven-cell
   manifest as its own campaign — 7/7 `Optimal` × 5 repeats, 35/35
   independent checks pass
   ([record](../../evidence/bench04-campaign-2026-10-03.json)).
- Follow-up 2026-10-03 — post-campaign CI regression caught and fixed
   ([record](../../evidence/lp-refactorize-perf-2026-10-03.json)).
   Commit `3662cac` refactored the basis from scratch after **every**
   accepted pivot, tripling LP-heavy Debug/TSan/ASan test costs until
   five CI jobs exceeded their budgets (Release stayed green, proving
   per-pivot cost rather than a path change). Refactorize is behind the
   `needs_refactorization()` trigger again with the
   `basis_solves_cleanly` pivot gates retained; 122/122, LP-01 17/30
   and the seven CLI cells re-verified. BENCH-04's pinned binary is
   superseded by this fix; its rows stand as measured.
 - Benchmark: [`evidence/lp-sparse-rss-20260930.json`](../../evidence/lp-sparse-rss-20260930.json)
  — retired dense dispatch shape vs sparse-first, forked VmHWM per
  (instance, path). Dense peak ≥ sparse peak on all 18 records; the
  synthetic 4096×12096 case measured 398,492 KB vs 8,192 KB
  (Δ 390,300 KB). No speed claim.

## 6. Change procedure

Any change to this contract (a changed envelope, a new dense site on the LP
dispatch, a changed fingerprint definition) must update this page in the same
change, keep `tests/stop_reason_test.cpp` and
`tests/lp_sparse_differential_test.cpp` green, re-run `ctest -j8`, and — if
the dimension envelope or stop attribution changes — update
[resource-limits.md](resource-limits.md) §3–§5 under its own change
procedure. Reported strings in §3 are part of the contract; they may not be
renamed without a version bump here.
