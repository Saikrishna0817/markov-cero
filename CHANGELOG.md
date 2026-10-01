# Changelog

## Unreleased

### 2026-10-02 release gate — hosted CI green end to end, a real GPU device run, and budgets sized from measurements

- **The hosted matrix is green** — [CI run 36938268360](evidence/hosted-ci-36938268360-2026-10-02.json)
  on `7fbb3ff`: **12/12 jobs success** (gcc/clang × Debug/Release/ASan-UBSan/TSan
  each at 121/121 CTest + Netlib 7/7 + MIPLIB 3/3, plus wheel, docs/web,
  CUDA-compile and ML-fallback). Four pushes were needed to get there and
  the record keeps all four: `f13ff97` (9 jobs failed), `e75201d` (6),
  `7d99223` (2), then green. Every fix widens a resource allowance or fixes
  detection/reporting — no assertion, tolerance or PASS bar moved.
- **What was actually broken** — the 180 s default watchdog killed
  `refinery_domain_and_iis` in four jobs (hosted worst 607 s) → `TIMEOUT
  1200`; the domain tests' original solver limits (60 s / 90 s) starved
  Debug builds → non-Release gets 360 s solver / 420 s deadline (hosted
  worst 116 s); the MIPLIB runner's flugpl search needs ~5x wall time under
  Debug or a sanitizer (5583–6703 of 12547 nodes at 60 s) → every
  non-Release CI build now runs with `--time-limit 600 --proof-time-limit
  300 --timeout-sec 1200` while Release keeps the defaults (same PASS bar);
  the hosted RLIMIT_AS lift keyed on an `AddressSanitizer` banner and missed
  TSan's silent signal death → `web/backend/sanitizer_build.py` identifies
  sanitizer builds by static-runtime markers or `ldd`, and the lift prints
  the hosted-limits §3 note it performs; and clang+TSan force-loads its own
  `operator new`, colliding with the allocation-injection harness at link
  time → those two tests link with `--allow-multiple-definition` and exit
  **77 (skip)** only under clang+TSan (gcc+TSan still runs them, 2/2 green).
- **A real device run** — [GPU device run](evidence/gpu-device-run-2026-10-02.json):
  the CUDA build compiled in the digest-pinned `nvidia/cuda:12.6.3-devel`
  container (arch 75; the container has no driver, so it compiles only) and
  executed on this host's **RTX 2050** (driver 610.57.04, CC 8.6):
  **121/121 CTest with 0 skips**, all 11 GPU-related tests Passed, buffer/
  CSR/refinery roundtrip probe lines captured. Hosted CI still has no GPU —
  its 8 device tests skip by design (11/11 with 8 Skipped in the CUDA job).
  No speed claim; one host, one run.

### 2026-10-02 benchmark — the BENCH-02 primal re-check failures closed as a reporting defect

- **All ten rows analyzed** — [record](evidence/bench02-primal-recheck-defect-2026-10-02.json):
  BENCH-02's 10 independent primal re-check failures (`QPLIB_0010` ×5 at
  1.28e-6, `capitanescu_dc_opf` ×5 at 7.50e-6 on the harness's first violated
  row) were not solver-verification failures: the production JSON field
  `maximum_primal_violation` was a **defaulted 0** on the QP path while
  `diagnostic.primal_residual` and the row slacks carried the exact real
  value, and the campaign harness reads that JSON field (campaign contract
  §8). The three tolerance scales that legitimately disagree here are laid
  out from the serialized primal: the harness's 1e-6 flat bar, the scale-aware
  solve gate (3.00000128e-06 allowance on the offending QPLIB row), and the
  documented 1e-4 engine certificate (convex-qp §2.4).
- **The fix reports the measurement** — commit `e75201d`
  ([src/api/engine_qp.cpp](src/api/engine_qp.cpp)) fills `primal_report` at
  the engine-gate scale so the report can never contradict
  `original_verified`, with [tests/qp_primal_report_test.cpp](tests/qp_primal_report_test.cpp)
  pinning it. Post-fix, QPLIB's serialized field equals the harness's
  independent measurement to every printed digit, and capitanescu's true
  maximum (5.12e-05 on `balance_5`) is now visible instead of a zero.
- **No verdict moved** — all 10 rows stay `independent_primal_failure`; the
  campaign was not re-run, no row was dropped or reclassified, and neither
  the 1e-6 harness bar nor any solver gate was loosened (numerical-policy
  rule 5 / campaign §7). The sibling gap in the nonlinear engines —
  `engine_nonlinear.cpp` never fills `primal_report` and `nlp_verifier.cpp`
  folds bounds into `inequality_violation` — is documented as open, not
  papered over with an invented mapping.

### 2026-10-01 release gate — dependency pinning and a known-vulnerability scan

- **Pinning** — one of REL-01's undone gate items, closed as far as this host
  goes. The qualified environment is frozen as 34 exact `==` versions in
  [`requirements/qualification.txt`](requirements/qualification.txt) (`pip freeze`
  of the `.venv` that ran the 120/120 CTest, 27/27 binding tests and both
  campaigns; no hashes, and the header says so). `[build-system]` in
  [`pyproject.toml`](pyproject.toml) is now bounded instead of open-ended
  (`setuptools>=77,<85`, `pybind11>=2.12,<4`, `cmake>=3.25,<5`) — exact `==`
  pins were rejected because setuptools 84 needs Python >=3.10 while the project
  declares `requires-python = ">=3.9"`, so an exact pin would make the wheel
  unbuildable on 3.9. CI in [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
  pins its actions to full commit SHAs (`checkout`, `setup-python`,
  `setup-node`), the CUDA container to tag **and** digest
  (`12.6.3-devel-ubuntu24.04@sha256:392c0df7…`), and `pip`, `pytest` and `numpy`
  to their qualified versions. CI's interpreter moves 3.11 → **3.14.7**, the
  version the qualification ran, which the pins require anyway (`numpy==2.5.3`
  needs Python >=3.12); the action SHAs were read back from upstream
  (`git ls-remote … refs/tags/v4|v5`) and each equals the tag's current
  target, and the container digest is Docker Hub's for that tag. What stays
  unpinned (apt inside the container, the `ubuntu-latest` label, the host
  toolchain, the `torch>=2.9` floor, npm) is
  written down in
  [`docs/guides/RELEASE.md`](docs/guides/RELEASE.md) §9.1 rather than implied.
  Verified by building a wheel under build isolation with the new bounds.
- **Scan** — [`evidence/vulnerability-scan-20261001.json`](evidence/vulnerability-scan-20261001.json):
  `pip-audit` 2.10.1 from an isolated venv outside the repository, run against
  the freeze on **both** the PyPI and OSV advisory services (two sources, same
  answer). 32 of the 34 pins audit clean; the two that cannot be resolved
  against PyPI — `markov-cero` (local wheel) and `torch==2.9.1+cpu` (a local
  version PyPI does not carry) — are silently skipped by a naive run, which is
  exactly how a clean-looking scan hides the one package at risk, so torch was
  re-audited at its PyPI release `2.9.1`: **4 known vulnerabilities**
  (`PYSEC-2025-194`, `PYSEC-2025-195`, `PYSEC-2026-139`, `PYSEC-2026-2286`;
  fixes at 2.10.0/2.13.0 for three, **no upstream fix** for CVE-2026-4538).
  All four sit in the optional `ml-training` extra, which the wheel never
  imports; they are recorded as open and accepted — inline in
  [`pyproject.toml`](pyproject.toml) and in the guide — instead of "fixed" by
  raising a floor that has never been qualified. Not claimed: any npm, C++,
  apt, container or driver scan, any hash verification, any signature, or
  anything about advisories published after that date.
- The SBOM generator's limitation lines now *name* the scan record and the
  freeze when they exist instead of flatly denying both, and `cmake` joins the
  recorded build dependencies; `evidence/release-sbom-20261001.json` keeps its
  original wording, because a generated record is never rewritten.
  [`docs/guides/RELEASE.md`](docs/guides/RELEASE.md) §9.1/§9.2 and §12,
  [`docs/project/STATUS.md`](docs/project/STATUS.md) and
  [`evidence/INDEX.md`](evidence/INDEX.md) carry the same scope limits.

### 2026-10-01 release gate — reproducible-build comparison

- Closed one of REL-01's explicitly undone gate items on this host: two
  independently configured clean build trees
  (`-DCMAKE_BUILD_TYPE=Release -DMARKOV_CERO_WARNINGS_AS_ERRORS=ON`)
  of commit `da13130`, created at different absolute paths, produced
  **107/107 byte-identical artifacts** — every executable plus the
  static library each tree builds, with 0 differing and 0 files unique
  to one tree — and each tree passed **120/120 CTest**. Recorded in
  [`evidence/reproducible-build-20261001.json`](evidence/reproducible-build-20261001.json)
  with the per-artifact sha256 map, toolchain versions and the artifact
  definition, and cross-referenced from
  [`docs/guides/RELEASE.md`](docs/guides/RELEASE.md) §12 and the
  [status record](docs/project/STATUS.md). The comparison covers the
  build tree only: no second host, no other toolchain, no wheel or
  prefix package, no signing, and no claim at any other commit — the
  signed-artifact, vulnerability-scan, dependency-pinning, D18 owner
  and second-host gates stay open and are not softened by this.

### 2026-10-01 GAP-01 — close the gaps the BENCH-01 campaign surfaced

- New binding contract
  [`docs/contracts/mps-input.md`](docs/contracts/mps-input.md) (v1),
  written before the code: an `RHS` entry naming the objective (`N`)
  row is the free-MPS objective constant and lands in
  `model.objective_offset` (so a reported objective is `cᵀx + offset`
  exactly as every engine and verifier already computes it, and the
  offset joins the model fingerprint); `RANGES` on the objective row,
  rim values on any other `N` row, duplicate entries and multiple rim
  vectors stay rejected with named messages rather than dropped. The
  contract also states what it does *not* claim: that either newly
  parsing model solves.
- The reader implements it (`src/io/mps_records.cpp`,
  `src/io/mps_model.cpp`): the previous blanket
  "objective-row RHS/RANGES values are unsupported" rejection — the
  cause of the two `InvalidModel` rows BENCH-01 reported for
  `data/netlib/e226.mps` (offset `-7.113`) and `data/netlib/grow7.mps`
  (offset `0`) — is replaced by the §1 rule, and `Parser` accumulates
  the offset into `Model::objective_offset` at build time. The
  `mps_parser` test's old "objective RHS ambiguity" rejection case is
  replaced by the four §3 obligations (offset applied, duplicate
  rejected, objective `RANGES` rejected, non-objective `N` row
  rejected) plus a parse of both netlib files with their expected
  offsets, under the CTest `MARKOV_CERO_SOURCE_DIR` environment the
  shared group already sets.
- Both models now reach the solver instead of failing at the parser —
  `e226` reports `NumericalFailure` (phase I) and `grow7`
  `IterationLimit` (phase II) at a 60 s cap. That is honest progress,
  not a solve claim: their 2026-10-01 campaign rows keep their
  `InvalidModel` status, because a recorded campaign row is never
  rewritten; only a new preregistration can re-run them. Full suite
  after the change: 120/120 CTest, 27/27 binding tests.
- Answered the second gap instead of restating it: `etamacro` was run
  through all four CLI engines on HEAD `e8a68be` **and** on the
  pre-LP-01 revision `32be1ec` with identical flags, and all eight
  runs return byte-identical status, objective, iteration count and
  message across the two revisions. Its `NumericalFailure` is
  therefore pre-existing — the open question
  [`docs/project/STATUS.md`](docs/project/STATUS.md) carried — and the
  cause is numerical, not dispatch: canonical complementarity 2.15e-3
  against a tolerance three orders tighter on a model with estimated
  condition 8.3e17, so the witness check declining to certify is the
  contract working. Recorded in
  [`evidence/gap01-etamacro-2026-10-01.json`](evidence/gap01-etamacro-2026-10-01.json)
  with both binaries' hashes, added to the deferred set in
  [`docs/contracts/sparse-lp-path.md`](docs/contracts/sparse-lp-path.md) §5,
  and indexed. The same section now records that the LP-01 four
  reproduce differently by path — `blend` fails only through the
  library differential harness and solves `Optimal` through the CLI —
  so each observation is read with its own path. No fix, no objective
  claim and no dense/sparse differential for `etamacro` (it is not in
  the frozen 17-fixture set); a pre-LP-01 build tree was used only for
  this comparison and is not part of any release artifact.

- Third gap closed with a second campaign instead of an edit: the nine
  post-freeze `hash_mismatch` cells can only run under a new
  preregistration, so `scripts/freeze_successor_instances.py` wrote
  `evidence/frozen-instances-2026-10-01.json` — the successor the
  campaign contract §3 allows — after re-hashing all 63 present
  instances (54 identical, 9 changed, 0 missing, 0 newly appeared) and
  stamping it with `successor_of` and the nine old/new digests. The
  2026-09-28 freeze is untouched. The accompanying
  [`evidence/bench02-freeze-note-2026-10-01.json`](evidence/bench02-freeze-note-2026-10-01.json)
  proves what can be proven: for seven of the nine the frozen bytes are
  exactly the pre-comment blob, commit `a9c7e42` added only `*` lines,
  and comment-stripped bytes have not moved since. For
  `process_network_large` and `refinery_scheduling_large` the frozen
  bytes are in no commit and no blob of this repository (checked under
  four normalizations), so their equivalence to the 2026-09-28 content
  is **not claimed** — and that is recorded, not glossed.
  BENCH-02 itself (`evidence/bench02-preregistration-2026-10-01.json`
  written first, then 1,325 rows, then the §6 summary) differs from
  BENCH-01 only in manifest, pinned binary (Release + `-Werror`, sha
  `36284e66…`), campaign id and timestamp: subset, order, caps,
  threads, repeats, tolerances and metric quantity are audited
  identical in the freeze note. Measured on the same host in ~1 h 42
  min: **0 `hash_mismatch`, 315 `ok` rows (all 63 present cells × 5),
  1,010 `absent`, 0 harness exceptions, 0 parent timeouts**; solved
  fraction **38/265 (0.143)** vs 31/265 (0.117) — seven newly solved
  (`capitanescu_dc_opf`, `li_crude_blending`,
  `neiro_refinery_scheduling`, `pochet_lot_sizing`,
  `process_network_large`, `refinery_scheduling_large`,
  `shapiro_network_flow`), none lost, MILP 2/149 → 9/149 while LP
  25/98 and QP 4/18 are unchanged; 0/63 status or objective
  disagreement; reference agreement 0/60 LP at 1e-5 and 60/75 MILP at
  1e-4 where the 60 are `ResourceLimit`/`NumericalFailure` rows that
  never claimed optimality. Independent primal re-check **200 pass,
  10 fail, 105 `not_checked`**: `QPLIB_0010` (5, 1.28e-6, unchanged)
  plus the new `capitanescu_dc_opf` (5, 7.503556e-6 — the solver
  reports `Optimal` and `VERIFIED` in canonical scale while the
  harness's original-scale reader exceeds its 1e-6 bar); both stay
  failures and no status is rewritten. `e226`/`grow7` now reach the
  solver (`NumericalFailure`/`IterationLimit`) instead of
  `InvalidModel`, the LP deferred set is unchanged, and three corpus
  caveats are published rather than hidden: two frozen files are
  unrecoverable, four declared-MILP `cases` cells are LP/QP to the
  solver's own classifier (caps and class metrics follow the declared
  class), and no speed ratio is computed between campaigns —
  different binaries, interfering workers, one host.

### 2026-10-01 blueprint REL-01 — package, qualify and document the supported solver

- Published the supported matrix and the install/upgrade/rollback
  procedure in the new
  [`docs/guides/RELEASE.md`](docs/guides/RELEASE.md): the D17
  platform tiers (one Linux x86-64 gcc/Release/Python 3.14 host, plus
  a locally sanitizer-qualified build; Windows, macOS, aarch64, CUDA,
  manylinux and other Python versions explicitly unsupported), honest
  D18 placeholders (`UNASSIGNED` support and security contacts, no
  SLA), the offline wheel and CMake-prefix install paths, the upgrade
  caveat that `--force-reinstall` is mandatory because rebuilds share
  version `0.5.2`, the validated rollback sequence, the hosted
  service's controls, the SBOM command, an explicit "what is NOT
  supported" section and the open IR-33 / G7 gates. Registered in
  `scripts/check_docs.py` and the documentation index.
- Hosted service (blueprint step 6) with contract
  [`docs/contracts/hosted-limits.md`](docs/contracts/hosted-limits.md)
  **v1.1**: a fixed 60-second request quota keyed by client address
  and checked before authentication (HTTP 429), `GET /metrics`
  monotonic counters whose snapshots are taken before their own
  response is counted, and a result-field whitelist so unknown payload
  keys never reach a client. Every v1 limit row is unchanged. New CTest
  `hosted_service_limits` joins `hosted_os_limits`; a new
  `web/backend/service_limits.py` holds the quota and counter logic
  with a loopback HTTP test double, and `web/backend/server.py` was
  split so the adapter stays readable. The numerical-policy contract's
  HTTP-adapter migration row is marked resolved.
- Recorded that a sanitizer build cannot run under the declared 1 GiB
  `RLIMIT_AS` (AddressSanitizer reserves ~14 TB of shadow address
  space). The contract's not-claims now say so, and the two hosted
  tests lift only that bound — printing that they did — when pointed
  at a sanitizer binary. Deploying a sanitizer build is not a
  supported configuration and the adapter itself lifts nothing.
- Qualification evidence for commit `980687c`:
  [`evidence/packaging-qualification-20261001.json`](evidence/packaging-qualification-20261001.json)
  plus raw configure/build/ctest logs and the prefix and rollback drill
  transcripts in
  `evidence/packaging-qualification-20261001/`. Clean Release build with
  `MARKOV_CERO_WARNINGS_AS_ERRORS=ON` → **120/120 CTest**; offline host
  wheel `markov_cero-0.5.2-cp314-cp314-linux_x86_64.whl` (sha256
  `c1f19c9d…79d91`, 4,416,813 bytes) installed into a fresh venv →
  **27/27** binding tests run from outside the repository; CMake prefix
  install of 93 files with every `bin/` and `lib/` byte-identical to the
  build tree and an installed external consumer PASS; prefix drill
  P1 install / P2 expected configure failure / P3 reinstall / P4 remove;
  rollback drill D1–D4 restoring the prior MINLP-01 wheel's `_core*.so`
  byte-identically, with the one old-wheel test failure explained as
  MINLP-02 version skew and the prior build passing its own suite 26/26
  (D1b). An ASan+UBSan build passed **120/120** (457 s).
- Step 4: new
  [`scripts/generate_release_sbom.py`](scripts/generate_release_sbom.py)
  writes an SPDX 2.3 document
  ([`evidence/release-sbom-20261001.json`](evidence/release-sbom-20261001.json))
  and a per-file source manifest
  ([`evidence/release-source-manifest-20261001.csv`](evidence/release-source-manifest-20261001.csv))
  regenerated against the release commit, embedding the wheel and
  installed-binary hashes. Offline and deterministic apart from the
  timestamp; no signature, provenance attestation, vulnerability scan,
  dependency pinning or reproducible-build comparison, and those
  absences are recorded in the document's own limitations.
- Still open and deliberately unclaimed: second-host installation and
  reproduction, independent release/security review (IR-33 / gate G7),
  two named maintainers plus a security contact (D18), hosted CI
  sanitizer and CUDA evidence (the workflow exists, no run record does;
  `nvcc` is absent on this host), signed artifacts, vulnerability
  scanning and dependency pinning. No production ML, NLP, GPU or MINLP
  support is advertised by this package.

### 2026-10-01 blueprint BENCH-01 — preregistered benchmark campaign

- Added the binding campaign contract
  [`docs/contracts/benchmark-campaign.md`](docs/contracts/benchmark-campaign.md)
  (v1) before any campaign code: the declared subset and its frozen
  order (§2), per-class caps, one thread, five repeats per cell with
  repeat 0 cold and round barriers (§4), the row schema and the
  `run_state` table with exit codes explicitly *not* treated as errors
  (§5), `/proc/<pid>/status` `VmHWM` as the only usable peak-RSS
  source because `os.wait4` returns the parent's high-water mark
  (§6), the metric definitions with numerators and denominators (§6)
  and the tolerance table (§7). §6 records the clarification that
  `solved_fraction` is a cell-level fraction — the original wording
  was unit-inconsistent, the preregistration's quoted text is not
  edited, and no declared subset, cap, thread, repeat or tolerance
  changed with it.
- New harness under [`scripts/run_bench01_campaign.py`](scripts/run_bench01_campaign.py)
  and `scripts/support/bench01_{config,mps,runner,prereg,summarise}.py`:
  `preregister` writes the declaration once and refuses to overwrite
  it, `run` executes one row per cell × repeat with resume validation
  against the recorded solver hash/threads/caps, `summarise` emits the
  JSON report. The independent MPS reader
  (`bench01_mps.py`) re-reads every model — integer markers by
  structure, blank RHS vector names, repeated row/column entries
  summed, `QUADOBJ`/`QMATRIX` accepted and ignored for activity
  purposes — and recomputes row and bound activity from the reported
  primal; a solver-reported status is never rewritten, and an
  `Optimal` row whose recomputed activity misses the harness bar is
  recorded as `independent_primal_failures` with a
  `harness_feasibility_failure_solver_reported_verified` note.
- Preregistration
  [`evidence/bench01-preregistration-2026-10-01.json`](evidence/bench01-preregistration-2026-10-01.json)
  written before any run: 265 declared cells (63 present, 202
  absent), split order holdout→tune→train, caps LP/QP 60 s and MILP
  300 s, one thread, 5 repeats, four concurrent children with the
  interference caveat, 27/63 reference-optimum coverage and four
  named limitations.
- `bench01_harness_selftest` (CTest, [`tests/bench01_harness_selftest.py`](tests/bench01_harness_selftest.py))
  asserts all six `SELF_TEST_OBLIGATIONS` — declared denominator,
  absent cells kept in it, cold-index assignment, exit code never
  used as an error, `ru_maxrss` documented as unusable, tolerance
  table binding — plus the status-never-rewritten rule and that no
  fabricated record reaches `evidence/`. CTest 118/118.
- First campaign: 1,325 rows, 0 harness exceptions, 0 parent
  timeouts ([summary](evidence/bench01-campaign-2026-10-01.json),
  [rows](evidence/bench01-campaign-2026-10-01.csv),
  [log](evidence/bench01-campaign-2026-10-01.log)). 270 `ok` rows
  over 54 instances, 1,010 `absent`, 45 `hash_mismatch`; solved
  fraction 31/265 = 0.117; 0/54 cells disagreed on status or on
  objective at 1e-6 across repeats; 165/170 independent primal
  re-checks pass, the 5 failures being `QPLIB_0010` at 1.28e-6
  (above the harness 1e-6 bar, below the solver's 1e-4 QP gate);
  reference agreement 0/60 LP and 0/10 MILP `Optimal` rows
  disagree, the 60 MILP rows counted being `ResourceLimit` and
  `NumericalFailure` rows that never claimed optimality. Nine
  `data/cases` files hash-mismatch because commit `a9c7e42` gave
  them comment headers the day after the freeze — declared,
  kept in the denominator, not executed. Statuses as measured; one
  host, one binary, no second solver, no speed claim.
- New gaps surfaced: `data/netlib/e226.mps` and `grow7.mps` are
  rejected `InvalidModel` with "objective-row RHS/RANGES values are
  unsupported", and the `NumericalFailure` set here (`bore3d`,
  `etamacro`, `scsd1`, `scsd6`) is not the LP-01 deferred set.
- Documentation: new guide
  [`docs/guides/BENCHMARKS.md`](docs/guides/BENCHMARKS.md)
  registered in `scripts/check_docs.py` and linked from
  `docs/README.md`; `evidence/INDEX.md`, `docs/project/STATUS.md`
  and this changelog record the run.

### 2026-10-01 blueprint MINLP-02 — independent OA proof replay

- Added the binding library contract
  [`docs/contracts/minlp-proof-replay.md`](docs/contracts/minlp-proof-replay.md)
  (v1) before any numerical code: the `MARKOV_OA_PROOF 1` format and
  budgets (§2), the obligation list O1–O12 in order (§3), the writer
  side (§4), the status/assurance downgrade table (§5.3, §5.4), the
  reader and verifier (§6), the test obligations (§7), the benchmark
  (§8), the documentation duty (§9) and the locked tolerances (§10).
  Obligation O2 binds **both** the model fingerprint and the source
  objective sense.
- New verifier core under [`include/markov_cero/verify/oa_proof.hpp`](include/markov_cero/verify/oa_proof.hpp)
  and `src/verify/` (`oa_proof.cpp`, `oa_proof_io.cpp`,
  `oa_proof_builder.cpp`, `oa_proof_internal.hpp`): `build_oa_proof`
  records master cuts, convexity pivots, the embedded MIP proof, the
  certified bound history and the incumbent; `verify_oa_proof` rebuilds
  the master rows from the **source** model (never reusing the solve
  path), replays every cut at `1e-8` with `1e-9` outward-only
  weakening, re-derives the pivot at `1e-6`, and re-checks incumbent
  integrality/feasibility/objective, bound and gap at `1e-6` with a
  `1e-9` gap cross-check over the locked `1e-3`. Reader and verifier
  poll the shared deadline and enforce `maximum_tangents = 10000`,
  `maximum_nodes = 10000`, `maximum_witness_values = 4e6` and a
  4096-byte fingerprint.
- §5.3 downgrade: a MINLP solve without an accepted **canonical** replay
  never reports `Optimal` — `not_requested` (proofs disabled),
  `exhausted` (budget) and `rejected` all fall back to `Feasible` +
  `original_primal_checked`, while an infeasible claim keeps
  `Infeasible` + `verified`. Accepted proofs publish `certificate_type`
  `incumbent_feasibility; independent_oa_gap` (`…_tree` at gap 0) and
  `out.best_bound`/`relative_gap` from the replayed record.
- Surfaces: `SolveResult::oa_proof` / `oa_proof_build_ms` /
  `oa_proof_verify_ms` ([`include/markov_cero/api/solve.hpp`](include/markov_cero/api/solve.hpp)),
  CLI JSON, the Python `oa_proof` key, and the standalone
  `markov-cero-verify-minlp` checker (exit 0 accepted, 1 rejected —
  including malformed and exhausted, 2 usage) with `--time-limit`,
  `--max-nodes`, `--max-values` and `--relative-gap`.
- `assurance` value `oa_replayed` emitted iff `proof_status ==
  "accepted"`, `guarantee_tier ∈ {independent_oa, replayed_oa}` and
  `canonical_verified`
  ([`docs/contracts/numerical-policy.md`](docs/contracts/numerical-policy.md)
  bumped to v3 with the migration note).
- §7 suites: [`tests/minlp02_proof_test.cpp`](tests/minlp02_proof_test.cpp)
  (healthy/infeasible/maximize round-trips, node- and time-limit
  downgrades, stripped-fingerprint `replayed_oa`, reader rejections),
  [`tests/minlp02_proof_attack_test.cpp`](tests/minlp02_proof_attack_test.cpp)
  (18 directed mutations naming the failing obligation plus a
  64-iteration seeded fuzz), and
  [`tests/minlp02_enumeration_test.cpp`](tests/minlp02_enumeration_test.cpp)
  (seeded convex quadratic MINLPs cross-checked against an exhaustive
  per-integer-pair oracle, plus an infeasible member). CLI sections in
  [`tests/repository_tools_test.py`](tests/repository_tools_test.py) and
  the binding section in
  [`python/tests/test_proof_guarantee.py`](python/tests/test_proof_guarantee.py).
  CTest 118/118, bindings 27/27.
- §8 benchmark: [`scripts/bench_minlp02_proof.cpp`](scripts/bench_minlp02_proof.cpp)
  (`minlp02_proof_benchmark`) runs the frozen four-case stratum through
  `api::solve_model` twice per case — proofs on and
  `enable_mip_proof = false` — recording status, assurance, tier,
  build/replay ms, `write_oa_proof` bytes and objective/bound/gap:
  4/4 accepted `independent_oa`, 6,821 proof bytes, proof-off arm
  `Feasible` + `not_requested`
  ([record](evidence/minlp02-proof-2026-10-01.json), listed in
  [evidence/INDEX.md](evidence/INDEX.md)). Wall difference disclosed
  only; no speed claim.
- Docs: [`docs/guides/VERIFY.md`](docs/guides/VERIFY.md) new OA replay
  section (format version, command, tier table, budgets, and the
  explicit limitation that floating-point replay is not a formal exact
  proof); `evidence/INDEX.md` row; `docs/project/STATUS.md` entry and
  risk-table update.

### 2026-10-01 blueprint MINLP-01 — outer-approximation cuts with source replay

- Added the binding library contract
  [`docs/contracts/minlp-oa.md`](docs/contracts/minlp-oa.md) (v1) before
  any numerical code: the restricted convex scope and screening (§2), the
  OA derivation (§3), cut provenance and outward-only weakening (§4), the
  independent source replay (§5), incumbent verification (§6), bound
  certification and the status map (§7), counters and caps (§8), the
  public interfaces (§9), the A–H test obligations (§10), the §11
  benchmark obligation and the numeric allowances (§12).
- New public cut API
  [`include/markov_cero/minlp/oa_cut.hpp`](include/markov_cero/minlp/oa_cut.hpp):
  `derive_oa_cut` evaluates the source polynomial (linear sense-signed
  terms, `NLOBJ` terms, and the quadratic energy assembled by
  `qp::make_quadratic_model`) at the linearization point and emits a
  gradient row with `kOaCutWeakening = 1e-9` added **outward only**;
  `replay_oa_cuts` re-derives each stored `OaCut` from the source model
  independently of the solving path (componentwise
  `kOaCutReplayTolerance = 1e-8` on gradient, value, rhs, weakening and
  sense); `oa_ineq_source_map` mirrors `make_nlp_model`'s row order.
- Two-path cross-check inside the OA loop
  ([`minlp_solver_oa_rows.cpp`](src/minlp/minlp_solver_oa_rows.cpp),
  [`minlp_solver_iterate_outer_approximation.cpp`](src/minlp/minlp_solver_iterate_outer_approximation.cpp)):
  rows are created from the NLP callback view, each newly stored cut is
  immediately replayed from the source polynomial, and any mismatch —
  or a row-map length discrepancy — fails closed to
  `numerical_failure` instead of trusting the master. Master rows carry
  the weakened rhs; `cuts_replayed` counts every replayed cut.
- Bound and status discipline (§7): only `optimal`, `gap_satisfied`,
  `feasible`, `iteration_limit` and `resource_limit` masters with a
  finite `best_bound` certify; an infeasible master with an incumbent
  fails closed to `numerical_failure` and without one reports honest
  `infeasible`; an unbounded master stops inconclusive at
  `iteration_limit` with **no big-M**; a certified bound never enters
  without `bound_provenance = "milp_master_certified"`, and every
  incumbent-retaining exit keeps only what §6 verified (integer
  residual ≤ 1e-6 plus `nlp::verify_nlp_feasibility`, no unverified
  binding, no bound without a certified master).
- New counters on `MinlpSolution` (additive): `sqp_calls`,
  `sqp_failures`, `master_nodes` (sum over all master solves),
  `cuts_replayed`, `bound_provenance`, and the full `oa_cuts` provenance
  records; `MinlpOptions::max_oa_cuts` (default 10000) stops exactly
  like the iteration limit when exceeded, and `0` is `invalid_options`.
  The MINLP engine branch now reports `nodes_explored = master_nodes`.
- Model screening: non-finite objective, matrix, `NLOBJ` and `NLCON`
  coefficients are rejected at `validate` time as `invalid_model`.
- §10 suites: [`tests/minlp01_oa_test.cpp`](tests/minlp01_oa_test.cpp)
  (case A analytic optimum with underestimator tangent checks at
  x = 0, 0.5, 1, the two-integer exhaustive oracle, infeasible-pair
  honesty in case F, master-node-limit honesty in case G, the OA row
  cap and `max_oa_cuts = 0`) and
  [`tests/minlp01_cut_replay_test.cpp`](tests/minlp01_cut_replay_test.cpp)
  (six directed corruptions of gradient/value/rhs/weakening/sense all
  rejected with the failing field named, 48 seeded random samples
  replayed against solve-time `oa_cuts`, and malformed-term/non-finite
  screening). CTest 115/115.
- §11 benchmark stratum: [`scripts/bench_minlp01_oa.cpp`](scripts/bench_minlp01_oa.cpp)
  (`minlp01_oa_benchmark`) runs four embedded restricted convex MINLP
  cases with known optima (basic 1.25, case-A binary 0.2, two-integer
  oracle 0.3, maximize+offset recorded in the normalized internal sense)
  through `minlp::solve_minlp` with a warm-up before each timed solve —
  4 Optimal, verified bounds within 1e-3, `cuts_replayed == cuts_added`
  on every case ([record](evidence/minlp01-oa-2026-10-01.json), listed
  in [evidence/INDEX.md](evidence/INDEX.md)). Solver-trusted bounds
  only; no speed claim.

### 2026-10-01 blueprint NLP-02 — elastic restoration for inconsistent SQP linearizations

- Added the binding library contract
  [`docs/contracts/nlp-restoration.md`](docs/contracts/nlp-restoration.md)
  before any numerical code: the frozen failure corpus measured on the
  post-NLP-01 tree (§2), the exact slack-penalized subproblem and penalty
  grid (§3), the original-violation acceptance rule A1/A2 (§4), the bounded
  inconclusive outcomes (§5), diagnostics (§6) and the §7 test / §8 paired
  benchmark obligations. `nlp-local-sqp.md` reaches **v2**: §4.4 now points
  to the restoration contract instead of deferring the work.
- **Frozen failure corpus (measured first):** the minimized feasible case
  `x2_minus_one` (`f = (x−1)²`, `h = x²−1 = 0`, start `x = 0` — feasible
  at `x = ±1`, but the zero-gradient linearization reads `0·d = 1`) failed
  with the NLP-01 deferral message; the truly infeasible `linear_pair`
  failed identically and must stay inconclusive.
- Elastic subproblem (`sqp_solver_subproblem.cpp`): slack columns
  (`n_ineq + 2·n_eq`), `P = diag(B_k, 0)` (stays convex), exact penalty
  `ρ = max(ρ, μ, 10)` escalating ×10 per rejected attempt (cap 1e8);
  constraint rows keep indices `0…m−1` so dual indexing is untouched.
- Restoration attempt (`sqp_solver_restoration.cpp`): trust-cap the `d`
  step at the shared `kTrustRadius`, backtrack `t = 1 … 2⁻¹⁰` accepting
  the first trial with a nonzero step (≥1e-10) and a ≥5% decrease of the
  **original** constraint violation — slacks never enter the test. On
  acceptance: iterate moves, counters update, no L-BFGS curvature, no
  multiplier overwrite, no convergence claim that step; on rejection the
  iterate does not move. After 5 consecutive rejected attempts the solve
  ends inconclusive (`restoration_exhausted`, `failure_site =
  sqp_restoration`) — never `Infeasible`, never `Optimal`.
- `SqpOptions::elastic_restoration` (default true) reproduces the exact
  NLP-01 immediate failure when disabled (the paired benchmark's old
  path); `SqpSolution` gains `restoration_steps`, `restoration_failures`,
  `restoration_exhausted`, and every exit message discloses
  `; elastic restoration steps=K` when K > 0. The binding exposes the
  `elastic_restoration` kwarg and both counters.
- Added the §7 suite [`tests/nlp_restoration_test.cpp`](tests/nlp_restoration_test.cpp)
  (recovery through the independent KKT checker, the disabled old path,
  the bounded honest failure with `best_feasible_x` empty, healthy-path
  regression with zero counters) and `test_nlp02_bindings.py` (recovery,
  off switch, key presence).
- Added the §8 paired benchmark: `scripts/run_nlp02_benchmark.py` runs the
  frozen eight-case corpus (NLP-01 §7 + `x2_minus_one`) twice through the
  public binding, on vs off — 7 vs 6 converged (+1 recovered failure),
  identical statuses and iterations on every shared-success case,
  `infeasible_pair` bounded at 6 rejected attempts
  ([record](evidence/nlp02-restoration-2026-10-01.json)). Paired
  in-process wall numbers only; no speed, globality or infeasibility
  claim.

### 2026-10-01 blueprint NLP-01 — local SQP honesty, callback guards and derivative diagnostics

- Added the binding library contract
  [`docs/contracts/nlp-local-sqp.md`](docs/contracts/nlp-local-sqp.md): the
  exact local guarantee (§1), the callback contract validated at every
  evaluation (§2), the developer-only derivative diagnostic (§3), the audited
  SQP iteration semantics with the documented Armijo/Wolfe waiver (§4), the
  status/verification/incumbent rules (§5), and the §6 test and §7 benchmark
  obligations.
- Status schema v2: the public NLP status `LocalOptimal` is renamed
  `LocalStationary` (numerical-policy v1 → v2 with a migration note); the
  C++ enum `local_optimal` is retained for source compatibility. A
  first-order KKT candidate — a saddle satisfies it — is stationarity, not
  a proven local optimum.
- Callback guard (§2): every solve-path constraint value/Jacobian and
  gradient/objective result is dimension- and finiteness-checked
  (`nlp_callback_guard`); violations and callback exceptions become a
  bounded `NumericalFailure` naming the callback (never a crash, never
  `LocalStationary`), a callback broken at report time fails closed to an
  unknown (`null`) residual instead of a plausible zero, and the solve
  counts and exposes user-callback evaluations.
- SQP semantics (§4): a `primal_infeasible` QP subproblem now fails
  immediately with an inconclusive message naming the linearized
  infeasibility (`B_k` affects only the objective, so a reset cannot help) —
  never `Infeasible`, never a claim about the NLP itself; x0 bound
  projection is disclosed (`x0_projected`, `x0_projection_norm`); the last
  feasibility-tolerated iterate is kept (`best_feasible_x`) and the engine
  attaches it only after its own `verify_nlp_feasibility` accepts it,
  without upgrading the limit/failure status.
- Added `nlp::check_derivatives` (public C++, plus `mc.check_derivatives`
  in the binding): centered finite differences in the interior, one-sided
  differences within one step of a finite bound, a documented step rule and
  error threshold — diagnostic only, never called inside the solve path.
- Added the §6 suites: `nlp_local_semantics` (two-start Rosenbrock, the
  analytic equality multiplier, bound-active stationarity through the
  bound-normal projection, saddle → `LocalStationary` never `Optimal`,
  infeasible pair stays inconclusive), `nlp_derivative_check` (wrong
  gradient/Jacobian detected, correct and bound-adjacent callbacks pass,
  NLOBJ polynomial path, malformed input fails closed) and
  `nlp_callback_guard` (NaN/wrong-size/throwing callbacks, x0 guards, a
  limit exit retaining a verified incumbent with projection disclosure, and
  the engine attaching that incumbent on a failed callback solve).
- Added the §7 benchmark: `scripts/run_nlp01_benchmark.py` runs a separate
  seven-case local corpus through the public binding — 6
  `LocalStationary`, 1 honest `NumericalFailure`, with residuals,
  iterations, callback counts and wall times as measured
  ([record](evidence/nlp01-local-2026-10-01.json)). Never merged with the
  QP/MIP/MIQP summaries; no globality, local-minimum or speed claim.

### 2026-10-01 blueprint MIQP-01 — certified MIQP node bounds, incumbent checks and replay obligations

- Added the binding library contract
  [`docs/contracts/miqp-node-bounds.md`](docs/contracts/miqp-node-bounds.md):
  the supporting-hyperplane node bound with its input-space invariant and
  fail-closed domain (§2), the node-QP dispatch and status map with the
  convexity gate (§3), incumbent verification from original quadratic data
  (§4), QP-leaf proof replay (§5), scope limits (§6) and the §7 test and
  benchmark obligations.
- Fail-closed bound domain (§2.3): `src/qp/supporting_bound.cpp` checks the
  iterate dimension before the quadratic matrix product, so a short witness
  returns −∞ instead of throwing out of the bound routine.
- Added the test suite: `miqp_supporting_bound` (independent rational
  restatement of §2.2 with directed guard-weakening, non-finite endpoint and
  input-space boundary cases), `miqp_node_bound` (box overlays in both
  endpoint directions, the infinite-endpoint fail-closed unbounded case,
  the §13 case A/B objectives, indefinite and nearly-PSD rejection at
  `numerical_failure`, and the maximize + offset sign path),
  `miqp_incumbent` (engine-gate and replay tamper rejection: inflated /
  deflated objectives, non-integral primals, forged proofs and objective
  edits), `miqp_proof_attack` (twelve proof-record attacks — altered
  primal/dual/objective, forged Farkas, relabeled leaves, split forgeries,
  fabricated incumbents — plus an accepted control), and `miqp_bruteforce`
  (seeded differential against exhaustive enumeration, including the empty
  box that may report neither an optimum nor a bound).
- Added the §7.8 benchmark stratum: `scripts/run_miqp01_benchmark.py` runs
  five checked-in synthetic convex MIQP instances
  (`data/miqp`, generated by `scripts/generators/gen_miqp01_instances.py`)
  through the production CLI at a 60 s cap — 4 Optimal, 1 Feasible
  (`miqp_large_n40`), 0 process-level complete failures, all five
  `original_verified`
  ([record](evidence/miqp01-synthetic-2026-10-01.json), indexed from
  `evidence/INDEX.md`). Its own stratum, never merged with the linear MIP
  summary; statuses as measured; no speed claim.

### 2026-10-01 blueprint MIP-01 — certified MILP node bounds, partition and proof obligations

- Added the binding library contract
  [`docs/contracts/milp-node-bounds.md`](docs/contracts/milp-node-bounds.md):
  the prune-reason witness table (F1–F3, P1–P12), certified bound provenance
  with the downward/absolute prune guard, the exhaustive branch-partition
  rule with `evaluate_split`/`push_branch_children` as the one shared
  construction for both engines, lattice-preserving cut-derivation rules,
  and the §6/§7 proof-obligation and test obligations.
- Certified every bound prune (F1): `certified_dual_bound` derives node
  lower bounds from the canonical dual with an absorbing downward guard;
  unknown/non-finite bounds never prune, and both engines prune queue/bound
  comparisons through `milp::prune_guard()` so large-magnitude objectives
  cannot manufacture a false prune.
- Proved the branch partition (P6/P12): production child construction is one
  shared module (`src/milp/branch_partition.cpp`) used by the serial search
  and both parallel push sites; degenerate or unselectable splits count
  unresolved (counter surfaced in the stop message, `proven` blocked, held
  bound never overstated); both-gate rejections record
  `Result::empty_domain_nodes` in both engines and at the parallel root.
- Guarded cut obligations (M4): GMI/MIR rows pass
  `cut_row_lattice_preserving` (fractional basic/contributor columns skip
  generation), per-round pre-cut separation solutions are recorded in the
  parallel proof collector, and optimizer cut notes with non-finite or
  non-binding values are dropped before a replay can see them.
- Hardened proof replay: `verify_mip_proof` validates cut/propagation
  obligation structure (row/column domain, finiteness) after the fingerprint
  check; budgets report `exhausted`, never `accepted`; a stripped
  fingerprint downgrades an accepted artifact to `replayed_tree`, a
  mismatched one is rejected.
- Added the test suite: `milp_branch_partition` (partition exhaustiveness,
  push-vs-replay domains, P6 proven-blocking, both engines' empty-domain
  counters), `milp_cut_validity` (fully enumerated integer points including
  a non-integral lower bound and an upper-bound-only integer), adversarial
  proof attacks and §6 fingerprint tiers (`mip_adversarial`), brute-force
  enumeration cross-check (`milp_bruteforce`), and the §7.3 edge cases
  (`milp_edge_cases`: resource stop with a verified incumbent, nonzero-gap
  proof, infeasible-vs-resource, PDLP F1 bound, P3 nearly-integral, P10 cut
  revert).
- Added the §7.6 benchmark: `scripts/run_milp01_benchmark.py` runs the 15
  checked-in MIPLIB MILP models through the production CLI at a 60 s cap —
  2 Optimal, 1 Feasible, 11 ResourceLimit, 1 NumericalFailure, 0
  process-level complete failures
  ([record](evidence/milp01-miplib-2026-10-01.json), indexed from
  `evidence/INDEX.md`). Statuses as measured; no speed claim.
- Documented MIP proof scope and the `Feasible` / `GapSatisfied` /
  `Optimal` / `Unverified` difference in
  [`docs/contracts/numerical-policy.md`](docs/contracts/numerical-policy.md)
  §3 (labels and statuses unchanged — no version bump).

### 2026-09-30 blueprint QP-01 — convex QP contract, disclosure and KKT attack suite

- Added the binding library contract
  [`docs/contracts/convex-qp.md`](docs/contracts/convex-qp.md) (v1): the
  frozen Hessian/canonical convention (upper-tri CSC, full-symmetric
  energy), the three-outcome convexity classification with its factorization
  caps, KKT/ADMM/verification/symbolic-cache rules, the §4
  status–certificate–`failure_site` mapping, the §5 disclosure trio, and the
  §6 test/benchmark obligations (executed findings recorded in §6).
- Added the `backend_actually_used` disclosure (`cpu` / `cuda` /
  `cpu_fallback`) next to `recommended_backend`: `SolveResult` field,
  `engine_qp` set from the solver's own `gpu_path_active` gate, `engine_pdlp`
  passthrough of the PDLP result, and emission in CLI JSON and the Python
  results. The CLI payload struct moved from `apps/json_output.hpp` into
  `apps/json_data.hpp` so the 300-line limit stays green.
- Made the QP witness name conditional under contract §4:
  `certificate_type = convex_qp_kkt` is published only when the independent
  verifier accepted the witness (KKT verifier for `optimal`, Farkas witness
  for `primal_infeasible`, recession ray for `dual_infeasible`); rejected
  witnesses report `none` instead of an unconditional label. Infeasible and
  unbounded QP results now publish the same accepted-witness name as
  optimal ones, mirroring `canonical_lp_witness`.
- Added `tests/qp_kkt_attack_test.cpp` (with fixtures in
  `tests/qp_kkt_attack_models.hpp`, ctest `qp_kkt_attack`): altered, scaled,
  sign-flipped, missing and NaN multipliers, perturbed primal points,
  altered objectives, wrong active side, complementarity breaches and
  non-PSD `P` are all rejected by the independent verifier; edge statuses
  (zero Hessian, singular PSD, equality bounds, empty row, unbounded
  recession) carry the §4 certificates through `api::solve`; the GPU-request
  disclosure reports `cpu` vs `cpu_fallback`; identical pattern with
  different `P` values reuses the symbolic factorization
  (`symbolic_factorizations()==1`, `kkt_symbolic_reuse>=1`) and yields
  independently verified, differing objectives.
- Added the §6 benchmark: `scripts/bench_qp_kkt.cpp` measures convexity
  classification, KKT `L` fill at the initial penalties, ADMM iterations and
  ρ refactorizations per instance; `scripts/run_qp_compare.py` +
  `scripts/qp_compare_lib.py` compare against OSQP 1.1.3 and HiGHS 1.15.1
  with primal checks and 1e-4 objective agreement. Executed on the four
  tracked QPLIB instances: 4/4 markov `Optimal` + verified, 8/8 agreements
  (worst relative difference 2.1e-6), KKT fill 5–57 nnz over 17–49 ADMM
  iterations ([record](evidence/qp-kkt-bench-20260930.json), indexed from
  `evidence/INDEX.md`). The small tracked subset only — no breadth or speed
  claim.
- This slice passed 99/99 CTest and 22/22 Python binding tests.

### 2026-09-30 blueprint LP-01 — sparse-first LP path

- Added the binding library contract
  [`docs/contracts/sparse-lp-path.md`](docs/contracts/sparse-lp-path.md)
  (v1): the sparse-first dispatch rule, the "no uncapped dense allocation"
  rule, the 4096×16384 envelope with escaped `length_error` → `work_limit`,
  one sparse-content fingerprint definition, and the dense/sparse
  differential and benchmark obligations.
- Retired the unconditional dense conversion from `engine_lp`: the LP
  dispatch now runs a named pre-dispatch dimension check (same envelope,
  same `work_limit` mapping, `stop_reason_test` unchanged in behavior) and
  solves the sparse working model directly. `dense_conversion_bytes`, its
  memory charge and its `dense_convert` stage are gone; the resource
  contract §4/§5 wording was updated in the same change.
- Made `lp::dual` sparse-primary: `Session::resolve`, `fingerprint`,
  `make_basis_state` and `solve` take the sparse canonical model; the dense
  overloads remain as explicit adapters through one shared
  `transform::sparse_from_dense`. The reference, IPM and IPM-crossover
  dense sites delegate through the same adapter, and IPM crossover now
  runs directly on the scaled sparse model (no dense scaled copy, no dense
  unscaled copy).
- Added `tests/lp_sparse_differential_test.cpp` over the frozen netlib set:
  statuses, objectives, both witness boundaries, cross-canonicalizer and
  round-trip fingerprints, bidirectional warm-start interchange and
  row-permutation invariance all agree between entries (98/98 CTest).
- Measured the retired conversion with `scripts/bench_lp_rss.cpp`: forked
  VmHWM per (instance, path), dense shape ≥ sparse on all 18 records; the
  synthetic 4096×12096 case measured 398,492 KB vs 8,192 KB
  (Δ 390,300 KB). Record:
  [`evidence/lp-sparse-rss-20260930.json`](evidence/lp-sparse-rss-20260930.json).
- Triaged `bore3d`/`scsd1`/`scsd6`/`blend`: their end-to-end
  `NumericalFailure` reproduces with identical statuses and objectives on
  the pre-LP-01 revision (clean stash rebuild), and the two LP entries
  agree exactly on all four — a deferred pre-existing defect, not
  introduced by this task.

### 2026-09-30 blueprint RES-01 — resource stop contract and measured envelope

- Added the binding library contract
  [`docs/contracts/resource-limits.md`](docs/contracts/resource-limits.md)
  (v1): the stop-reason table, attribution rules R1–R5 (sticky first stop,
  every `resource_limit` describes itself, precise boundary attribution,
  honest `unspecified_resource_limit` fallback, empty off the resource path)
  and the non-guarantees (cooperative polling, instrumented charges only,
  byte/input caps are not RSS).
- Extended `StopReason` with `input_limit`, `work_limit` and
  `allocation_failure` (existing numeric values unchanged) and mapped parser
  byte caps, escaped `length_error`s and host `bad_alloc`s at the API
  boundary. `stop_reason` is non-empty on every `resource_limit` result in
  C++, CLI JSON and Python, and never accompanies an unverified
  optimal/infeasible/unbounded claim.
- Added shared-context polls at model hashing, classification, the
  post-engine completion sweep around row activities, PDLP/QP/nonlinear
  verification, the MIP proof build, and the serial MILP search's root and
  per-node boundaries (new `milp::Options.context`).
- Charged the serial frontier against the shared memory budget (root and
  both child pushes, mirroring the parallel `sizeof(BranchNode)` convention)
  and exposed the admitted-charge high-water mark as
  `memory_charged_peak_bytes` in C++, CLI JSON and Python.
- Added boundary tests `tests/stop_reason_test.cpp` (input/work/allocation
  attribution, cooperative reasons, node-quota and engine-deadline
  attribution, queue capacity, shared cancellation, serial charge refusal,
  peak bounds) and `tests/sparse_fill_limit_test.cpp` (the factor fill cap
  throws before any partial factor exists).
- Measured the cooperative envelope with
  `scripts/bench_resource_envelope.py`: 15 trials across small and larger
  inputs under tight wall and memory limits. Worst observed deadline overrun
  2.3 ms; RSS under a 64 KiB charge cap stayed ~17 MiB — both exactly the
  contract's non-guarantees. Record:
  [`evidence/resource-overrun-rss-20260930.json`](evidence/resource-overrun-rss-20260930.json).
- Added the hosted hard-limit contract
  [`docs/contracts/hosted-limits.md`](docs/contracts/hosted-limits.md) and
  kernel-enforced limits in `web/backend/server.py` (rlimit wrapper for CPU,
  address space and file size, redirected output, kill-on-timeout) with
  `web/backend/os_limits_test.py` (CTest `hosted_os_limits`).
- Verified this slice: 97/97 CTest, 22/22 binding tests, and clean source,
  docs, JSON, backlink and sovereignty guards.

### 2026-09-29 blueprint NUM-01 — numerical and status contract (v1)

- Added the binding contract `docs/contracts/numerical-policy.md`: one
  documented meaning (quantity, units, formula, default, valid range, producer,
  checker) for every tolerance across simplex, IPM, PDLP, ADMM, SQP, MILP,
  presolve/scaling and the independent verifiers; the two verification
  boundaries (original model vs class-specific witness); the status-to-witness
  table; nine recorded divergences; the change procedure and the contract-v1
  migration note.
- Added a typed `SolveResult::assurance` label (C++, CLI JSON key `assurance`,
  Python dict key `assurance`) derived once in `api::detail::finalize` after the
  resource-stop rule: `tree_replayed`, `optimality_witness_checked`,
  `local_kkt_checked`, `original_primal_checked`, `unverified`. Additive only —
  no existing field, status, tolerance or default changed, and engines never
  write it.
- Added boundary tests `tests/numerical_policy_boundary_test.cpp` and
  `tests/assurance_label_test.cpp`: each accepted witness is mutated to both
  sides of the threshold it is checked against (original row, objective,
  integrality, canonical LP primal and duality gap, QP stationarity and PSD
  gate, curvature classification, NLP feasibility, KKT stationarity and dual
  sign), and the fail-closed label cases are asserted directly.
- Measured verifier overhead on eight sparse models with
  `scripts/bench_verifier_overhead.cpp`: both boundaries accepted every
  reference witness, at 1.5–99 µs per acceptance and 0.08–0.61 % of a full
  production solve. No speed target. Record:
  `evidence/verifier-overhead-20260929.md`.
- Verified this slice: 94/94 CTest, 22/22 binding tests, and clean JSON, docs,
  source-limit and backlink guards. Observations recorded for later tasks:
  `data/netlib/e226.mps` hits an unsupported objective-row RHS in the parser;
  `data/netlib/bore3d.mps` reports `NumericalFailure` on the production LP path
  (LP-01); `web/backend/server.py` still whitelists old result keys and does not
  pass `assurance` (REL-01).

### 2026-09-29 blueprint BASE-01 — frozen source, claims and release metadata

- Resolved the license conflict (owner decision D18): `pyproject.toml` now
  declares `Apache-2.0` with `LICENSE`/`NOTICE` shipped in the wheel, matching
  the checked-in Apache 2.0 license; build requirement raised to
  `setuptools>=77` for PEP 639 metadata.
- Added a `python-bindings` CI job that verifies license coherence, builds the
  wheel and runs `python/tests`, closing the gap that let a stale binding-test
  expectation go unnoticed.
- Corrected `python/tests/test_proof_guarantee.py` to expect `Feasible` after
  proof-budget exhaustion, per blueprint section 5 (proof exhaustion is never
  converted to `Optimal`). The solver behaviour was already correct; only the
  test expectation changed.
- Recorded the reproducible baseline in
  `evidence/baseline-manifest-20260929.json` with raw logs: clean Release build
  of revision `aa6f35e` (no C++ source changed), 92/92 CTest, 22/22 binding
  tests, installed-consumer proof replay PASS, and artifact SHA-256 hashes.

### 2026-09-29 repository layout and documentation

- Consolidated current guides under `docs/guides/`, status and provenance under
  `docs/project/`, the visual app under `web/`, and the qualification script
  under `scripts/`; updated CMake, package, Docker, CI and source links. Added
  a bounded Docker build context and a docs/web CI check.
- Replaced stale short guides with a detailed project README, current status
  summary, documentation index and example context. Dated research and evidence
  remain explicitly marked as historical records.
- Earlier cleanup removed obsolete audit/navigation material, orphan fixtures,
  an unused image and an unwired measurement source. The sparse-LU deadline
  regression is now registered and its cooperative deadline path compiles.

### Industry roadmap: solve-wide resource contract (backlog item 5)

- **Solve-wide limits** (`include/markov_cero/api/solve.hpp`, `src/api/`):
  `total_time_limit_seconds` and `memory_limit_bytes` options across C++, CLI
  (`--memory-limit-bytes`, plus `--time-limit` mapping) and Python; one `SolveContext` per solve
  threads the deadline, budget, quotas and sticky stop reason through every engine and verifier;
  `SolveResult::stop_reason` reports the shared stop.
- **Instrumentation and workers**: `StageScope` wraps each LP/QP/PDLP/MILP/parallel/nonlinear
  stage; parallel workers run under `WorkerContext` with per-charge accounting and local stops;
  the four maintained verifiers accept optional deadlines checked between iterations with
  documented bounded overrun.
- **Failure discipline**: budget refusals map to `resource_limit` (never `numerical_failure`),
  nothing escapes `solve_file`/`solve_model`, and allocation-failure injection sweeps hit LP,
  MILP, parallel, PDLP and QP engines; `scripts/worker_kill_demo.sh` demonstrates a SIGKILLed
  solver producing no false result with a clean rerun.
- **Gates**: 89/89 CTest (`build_item5`), 21/21 wheel pytest, source limits clean. Deadlines are
  cooperative and memory limits meter instrumented charge points only; IR-20/21 remain OPEN
  pending independent review (`evidence/resource-envelope-20260928.json`).

### Industry roadmap: proof guarantees (backlog item 6)

- **Phase 1 — proof metadata** (`include/markov_cero/verify/mip_proof.hpp`, `src/verify/mip_proof*`):
  assurance tiers (`independent_tree`/`replayed_tree`/`unverified`), consumed budgets (nodes,
  witness values, time, exhausted flag, budget kind), model fingerprint binding, report-carried
  build/replay timings, and exhausted-vs-rejected-vs-unsupported distinction; serialization bumped
  and round-trip tested with unknown-version rejection. Timing evidence recorded on 157-node and
  17-node production certificates (`evidence/proof-guarantee-20260928.json`).
- **Phase 2 — result surface** (`src/api/mip_certificate.cpp`, engines, CLI, Python):
  `SolveResult`, CLI JSON/text and Python expose the guarantee tier, proof status, consumed
  budgets, exhaustion and fingerprint binding. Exhausted proofs retain only the checked-incumbent
  certificate (`incumbent_feasibility`, tier `unverified`); accepted relative-gap proofs use
  `independent_mip_gap` and never claim exact optimality.

### Industry roadmap: frozen benchmark baseline (backlog item 7)

- Frozen comparator and instance manifests with deterministic family splits and timing constants
  as code (`scripts/support/frozen_timing_config.py`), then the pre-tuning baseline was
  regenerated under pinned hashes (`evidence/comparison/baseline_frozen_20260928/`,
  `evidence/benchmarks/baseline_frozen_20260928/`). Offline-only: missing optional datasets are
  listed unavailable, never downloaded (`evidence/baseline-freeze-20260928.json`).

### Industry roadmap: repeated-solve reuse (backlog item 8)

- **Session reuse** (`src/lp/dual/`, `src/qp/`): the dual-simplex session retains the accepted
  basis and factorization across repeat solves with cold fallback on invalidation or
  infeasibility; QP reuses symbolic KKT analysis gated on shape, fingerprint and exact sparsity
  pattern with correct invalidation; accepted LP and QP results pass an independent verifier
  before return.
- **Measured, not promoted**: 100-repeat LP warm-to-session time improved about 22% (300→233 ms);
  QP cold-to-session about 4–5% — below the roadmap 20% QP promotion gate, so engine wiring ships
  as opt-in sessions with numbers in `evidence/repeated-solve-20260928.json`.

### Industry roadmap: MILP strengthening and proof obligations (backlog item 9)

- Singleton-row propagation now produces `BoundEvidenceSource::propagated`; serial search records
  LP-bound, incumbent and remaining-search timings; verified cuts and applied propagations emit
  format-3 audit annotations accepted into replay-compatible proofs; parallel root-cut failures
  are reported and leave the root model unchanged; the worker-local in-tree cut helper and child
  cut inheritance are wired through the worker loop. Annotations are audit records — acceptance
  still comes from independent cut-free replay
  (`evidence/milp-strengthening-20260928.json`, `docs/codebase/technical-debt/mip-proof-obligations.md`).

### Industry roadmap: refinery units, quality and named reports (backlog item 10, code portion)

- Typed refinery input conversions with dimension-mismatch rejection (bbl/kbpd/d, t/kt, sulfur
  wt%↔ppm via density, RON, cetane, RVP, USD/bbl), a synthetic fuel-oil sulfur constraint,
  unit-checked FCC feed balance, and named JSON reports with unit-labelled variables, objective
  coefficients, rows, duals, slacks, active bound names, quality margins and row-scoped IIS
  entries (`include/markov_cero/refinery/refinery_{units,quality,report,report_json,input_schema}.hpp`).
- Synthetic/public-data qualification passed the refinery/domain tests; refinery engineer approval
  (IR-34/G34) and shadow-trial agreement (G8) were NOT obtained and remain OPEN
  (`evidence/refinery-units-schema-20260928.json`).

### Industry roadmap: packaging qualification, support and rollback (backlog item 11, code portion)

- Offline wheel and sdist qualification with fresh-venv install and pytest, CMake prefix install
  verified with an external `find_package` consumer, a performed upgrade/rollback drill restoring
  the prior wheel and re-running tests, extended `scripts/verify-release.sh` (report/env overrides,
  both compilers attempted), support/rollback documentation (`docs/governance/support-rollback.md`)
  and a release manifest with wheel SHA-256 (`evidence/packaging-qualification-20260928.json`).
- SBOM, signing, vulnerability review, support ownership and hosted evidence remain OPEN under
  IR-33/G7; the licensing metadata inconsistency (proprietary text vs Apache-2.0 LICENSE) is
  recorded for a human decision (D18).

### Industry roadmap: immutable node views and memory evidence (backlog item 4)

- **Bounded reference-materialisation comparator** (`include/markov_cero/milp/reference_materialisation.hpp`,
  `src/milp/reference_materialisation.cpp`): `compare_bounds_to_reference`/`compare_view_to_reference`
  replay caller-supplied branching steps chronologically against production's delta-chain walk — two
  independent algorithms, so the oracle needs no accessors on `NodeView` — with `ReferenceCaps`
  (variables, steps, total cut ids, bytes) refusing over-cap requests before any allocation
  (`reference_bytes == 0`), budget-charged `NodeView` materialization for view comparison, and
  facet/index mismatch reporting (`lower_bounds`, `upper_bounds`, `cut_scope`, `production`).
- **Tests** (`tests/reference_materialisation_test.cpp`,
  `tests/node_view_reference_identity_test.cpp`): 200 randomized chains with repeated writes, 400
  push/pop operations against chronological reconstruction, sibling bounds/cut isolation including
  parent immutability, refusal on every cap, exact mismatch facets and invalid inputs; bit-identical
  node-LP status/objective/primal from production-materialized and reference-replayed bounds, with
  original-space `verify_primal` certificates for node and full-solve results.
- **Frontier peak tracking** (`Result::max_queued_nodes`, `NodeFrontier::peak_size`,
  `ThreadSafeNodeQueue::peak_size`, `NodeBounds::retained_bytes()`): serial and parallel drivers
  record the peak frontier size so full-solver queue storage is measured rather than assumed.
- **Memory evidence** (`scripts/frontier_storage_bytes.hpp`, extended
  `scripts/bench_node_frontier_memory.cpp` with root/worker/queue byte classes and a matrix-fill
  parameter, new `scripts/bench_solve_frontier_memory.cpp` with parse/solve RSS split, a 10 ms
  resident sampler and a 250 ms signal-tick high-water): queue bytes are byte-identical across a 61×
  root-matrix growth and linear in node count; the materialized baseline queue reaches 169.5 MB at
  4,000 nodes versus 0.84 MB current; full-solver runs peak at 47–531 queued nodes with queue record
  bytes ≤ 63,720. All six W01 acceptance criteria are mapped and IR-19 is closed in
  `evidence/ir19-w01-memory-20260928.json`, which also records an unexplained transient ~1 GiB RSS
  anomaly (5/~35 runs, parse/root phase, queue excluded) handed to IR-21 follow-up.
- **Gates**: suite 89/89 in Release, ASan/UBSan with warnings-as-errors, and TSan; the locally built
  wheel passes 20 pytest cases; source limits (449 files), doc links and JSON checks pass. Open:
  worker-class storage (serial node-model workspace, per-worker overlays, sparse-PDLP fallback),
  per-node cut application, reusable factor work, persistent-structure caps, and solve-wide memory
  budgets (IR-20/21, backlog item 5).

### Industry roadmap: instrumentation and failure harnesses (backlog item 3)

- **Stage instrumentation** (`include/markov_cero/core/instrumentation.hpp`,
  `SolveContext::remaining_ms()`, `TraceEvent::worker`): a `StageScope` RAII timer emits
  exactly one trace event per stage — on early return and unwinding too — carrying the stage
  name, the shared sticky stop reason, the stage counter, net bytes charged during the stage,
  observed elapsed time and a worker index; deadline slack is reported live and never
  silently extended.
- **Worker isolation boundary** (`include/markov_cero/core/worker_context.hpp`): one shared
  `SolveContext` (deadline, cancellation, a single memory budget, the solve-wide stop reason,
  capability policy, seed, quotas, sink) versus per-worker charge attribution, release rights
  clamped to what that worker charged, and a worker-local sticky stop that stops only that
  worker. Refusals still fail the solve closed; destruction returns held bytes.
- **Allocation-failure harness** (`tests/support/failing_new.hpp`,
  `tests/resource_failure_test.cpp`): a global `operator new`/`delete` replacement in one test
  binary fails the nth allocation of a call under test exactly once (nothrow forms return
  `nullptr`). Coverage: harness self-check, `ModelSnapshot::capture` reporting and recovery,
  `NodeView::materialize` failing closed, and the API boundary — every injected failure across
  the exercised allocation points maps to `resource_limit` with failure site
  `allocation_failure`, with zero exceptions escaping `solve_file`/`solve_model` and never an
  infeasible or unbounded status.
- **Failure-semantics fixes found by the harness**: `src/api/api.cpp` catches `std::bad_alloc`
  at the boundary (previously it fell through the generic handler to `numerical_failure`) and
  wraps both entry points so nothing escapes; `MaterializationStatus::allocation_failed`
  distinguishes a host allocation failure from solve-budget exhaustion; `NodeBounds::materialize`
  builds its delta path before writing outputs so a failure cannot leave a half-written result.
- **Gates**: new CTest `worker_context` and `resource_failure` take the suite to 87/87 — in
  Release, under ASan/UBSan with warnings-as-errors, and under TSan; the locally built wheel
  passes 20 pytest cases; source-limit (443 files) and doc-link checks pass. Commands and
  limitations are recorded in `evidence/resource-instrumentation-20260928.json`. Open: no
  engine stage emits stage events, the API creates no `SolveContext`, the parallel search does
  not use `WorkerContext`, injection coverage stops at the exercised boundary points, and
  IR-20/21 remain open.

### Industry roadmap: shared contracts (backlog item 2)

- **SolveContext contract** (`include/markov_cero/core/`): sticky first-reason `StopReason`
  with failure semantics (a resource stop is reported as a resource/limit status, never as
  optimal/infeasible/unbounded), one absolute `Deadline` stages may only shorten, an
  overflow-safe fail-closed `MemoryBudget` for solve-wide allocation accounting, and a
  per-solve `SolveContext` carrying cancellation, thread/device quotas, seed, capability
  policy and a caller-owned trace sink — no globals, narrow accessors per stage.
- **ModelSnapshot contract** (`include/markov_cero/model/model_snapshot.hpp`,
  `src/model/model_hash.cpp`): validating capture by copy or move (snapshots are not
  copyable), separable structural/numeric FNV-1a hashes with a mixed stable fingerprint,
  and estimated solver-owned bytes for budget accounting.
- **NodeView contract** (`include/markov_cero/milp/node_view.hpp`, `src/milp/node_view.cpp`):
  immutable shared-parent node views holding only per-node bound deltas, structurally scoped
  cut IDs, bound-evidence provenance (`certified_relaxation`/`inherited`/`propagated`/
  `unknown`, NaN when unknown) and shared basis metadata, with explicit budget-charged
  materialization that fails closed on refusal, shape mismatch or an out-of-range delta.
- **Result binding**: `src/api/dispatch.cpp` hashes the validated model once at the API
  boundary and `SolveResult::model_fingerprint` binds every result to that model; covered by
  `tests/api_test.cpp`, exposed to Python as `model_fingerprint` (`python/src/results.cpp`),
  and covered by `python/tests/test_bindings.py`. New CTest gates `solve_context`,
  `model_snapshot`, `node_view`; fresh Release run passes 85/85 (also 85/85 under ASan/UBSan
  and under TSan), the wheel build passes its 20 pytest cases, and the source-limit and
  doc-link checks pass; commands and limitations are recorded in
  `evidence/contracts-slice-20260928.json`. These are contracts:
  engines still receive the mutable `Model`, no solve-wide byte limit is exposed or enforced,
  and IR-19/20/21 remain open.

### Phase 8 RW batch: keep the core, rebuild the edges

- **RW-1 (in-tree cuts)**: cut-loop tuning — separation frequency gating, per-node time/efficacy
  budget, and disable-on-regression so unproductive cut rounds cannot balloon runtime
  (`src/milp/milp_solver.cpp`); re-captured `evidence/cut_effectiveness.csv`.
- **RW-2 (parallel tree search)**: queue rework — best-bounded `pop_batch()` (16 nodes per lock
  acquisition), lazy prune-at-pop instead of heap-wide prune storms, 2 ms wait slices,
  interleaved batch consumption for soft work stealing. flugpl scaling 0.56× → **3.68× at 4
  threads** (4.34× at 8), TSan-clean; evidence with hardware manifest in
  `evidence/benchmarks/rw2_parallel_scaling.md`.
- **RW-3 (comparison harness, R16)**: added `scripts/run_compare.py` (markov-cero vs HiGHS
  1.15.1 via `highspy`), pre-registered instance lists `data/compare/{netlib,miplib}.txt`,
  Dolan–Moré profile plot, and a `compare_harness` CTest gate. Measured: 17/20 status+objective
  agreement gates pass; geometric-mean runtime ratio 42.6×; artifacts in
  `evidence/benchmarks/compare/`.
- **RW-4 (library API)**: added CMake `install()` targets for the `markov_cero` API headers and
  `markov-cero-solve`; verified with a clean-prefix install (R14).
- **RW-5 (single canonicalization path)**: sparse-first canonicalization confirmed as the only
  production path; dense path restricted to the verification oracle; scale ceiling of the dual
  engine's dense workspace precisely characterized and reported with explicit errors instead of
  silent caps (R12).
- **RW-8 (numerical robustness tooling, R9/R17)**: selective one-step iterative refinement in
  `SparseBasisFactorization` (extended-precision residuals; triggered by eta-chain length, growth
  factor, or pivot-ratio proxy), `sparse_condition_estimate()`, and a limitation test proving
  unit-pivot ill-conditioning evades pivot-based proxies. Ill-conditioned fixture: residual
  6.7e-15 → 1.9e-15 with two correction steps.
- **RW-9 (honest GPU claims, R8)**: rewrote `docs/gpu.md` — the crossover table is now generated
  from `crossover_study.csv` (13 rows; three previously quoted rows had no CSV backing), all
  ">50,000×" undefined-ratio rows removed, and the headline now leads with the engine-matched
  verdict (GPU loses 13/13 end-to-end, 0.34–0.87×).
- **RW-10 (claims layer)**: added the R1–R20 coverage matrix to `docs/project/STATUS.md` (12 MET / 8 PARTIAL,
  every PARTIAL naming its gap); rewrote `VERIFY.md` to describe what `verify-release.sh`
  actually does; added the shared hardware manifest `evidence/hardware.md` (PS-GAP-05).
- Fixed `src/api/api.cpp` missing from `CMakeLists.txt` (broke `markov-cero-solve` linking).

### Phase 5 Architecture Problems (AP-1–AP-12) closure

- **AP-1 (R4): interior-point engine + crossover** — `src/lp/interior/ipm.cpp`,
  `include/markov_cero/lp/interior/ipm.hpp`: infeasible-start Mehrotra predictor-corrector on
  the canonical standard form, normal-equation Newton systems under Ruiz equilibration, with a
  documented diagonal-perturbation fallback for ill-conditioned normal systems
  ([[Lustig-1992-Implementing-Meherotras-Predictor-Corrector]]). Crossover builds a candidate
  basis from the interior iterate by rank-revealing selection (large-x columns first, per-
  candidate independence test, rank-completion fallback — the canonical matrix has no full
  slack identity block, since equality rows carry no slack column) and certifies it through the
  dual simplex warm start ([[Ye-1998-Crossover-Interior-Point]]). New `--engine ipm` dispatch
  with the same presolve/scale/dual-gated verification as the simplex engines, plus an honest
  engine-level fallback to the reference primal simplex when the IPM cannot certify. New
  `tests/ipm_test.cpp` (vertex optimum, equality-row basis regression, basis reused as a dual
  warm start, crossover-disabled path) registered as CTest `ipm`. Netlib sweep: 16/17
  optimal+verified — 8 crossover-certified vertices, 8 honest fallbacks; the IPM also solves
  `blend`, `kb2`, `scsd1`, `scsd6`, which the dual engine cannot.
- **AP-9: presolve depth** — five new reduction classes beyond empty row/column and row
  singleton: forcing rows, duplicate rows, dominated duplicate columns, fixed variables, and
  zero-range infeasibility, each with a dual-restoring postsolve rule. The forcing-row dual is
  the minimum admissible multiplier over the incident columns,
  `pi_i = min_j (c_j - sum_{r != i} a_rj*pi_r) / a_ij`; the earlier `c_j/a_ij` form ignored the
  other rows' contributions and produced dual-infeasible witnesses (caught by the strengthened
  `tests/presolve_test.cpp`, which now checks full-witness dual feasibility).
- **AP-10: CI GPU coverage** — added a CUDA job; fixed the `run_gpu.py` status gate so the
  BLEND baseline failure is no longer silently reported as a pass (`gpu_benchmarks` /
  `gpu_profiling` now pass on their merits).
- **AP-11/AP-12: hygiene** — `docs/history.md` archived behind the CHANGELOG; dead options,
  duplicate runners and unwired fuzz targets removed; the RW-2 scaling script moved out of
  `benchmarks/runners/`.
- **Docs/claims layer** — `docs/project/STATUS.md` gains the AP-1–AP-12 closure register and R4 moves to
  **MET** (register now 13 MET / 7 PARTIAL); README documents the IPM engine; the research
  vault's `No Interior-Point Engine`, `No Crossover` and `Interior-Point Method` notes record
  their resolution. Full CTest sweep: **46/46 passing**.

## 0.5.2 — 2026-09-21

### Phase 6: Convex QP & MIQP (commit 255bf62)
- Implemented QuadraticModel canonical form with symmetric sparse CSC matrix storage.
- Implemented positive semi-definiteness check via dense and sparse LDLᵀ inertia inspection.
- Implemented Timothy A. Davis sparse LDLᵀ factorization (Algorithm 849, ACM TOMS 2005).
- Implemented OSQP-style operator splitting ADMM solver with over-relaxation and adaptive rho.
- Implemented analytical primal and dual infeasibility ray certificates (Banjac et al. 2019).
- Implemented independent zero-trust KKT certificate verifier (residuals and complementarity).
- Extended free-format MPS parser with QUADOBJ and QMATRIX quadratic objective sections.
- Integrated continuous QP relaxations and quadratic energy heuristics into branch-and-cut MIQP.
- Added CLI options: --engine qp and --engine miqp with automated quadratic model dispatch.
- Expanded automated test suite to 43 CTest targets with 100% pass rate.

### Phase 5: GPU Acceleration & Scale Crossover (commits 4ca75b2..3c5f356)
- Implemented sovereign CUDA first-order PDLP engine with device-resident iteration.
- Implemented DeviceBuffer RAII container and DeviceCsr sparse matrix formats.
- Implemented custom warp-per-row CSR SpMV and transpose-SpMV kernels.
- Implemented deterministic two-stage parallel reductions for vector norms and dot products.
- Implemented fused device-resident PDHG step (zero in-loop host-device transfers).
- Implemented adaptive restart on normalized duality gap and adaptive step-size scaling.
- Added relative KKT termination criteria at 1e-4, 1e-6, 1e-8 tolerances.
- Added four-part timing telemetry (H2D, kernel, D2H, total) in JSON output.
- Built run_gpu.py three-way comparison benchmark runner (Simplex vs CPU vs GPU).
- Executed a scale crossover study. The historical 29,320× GPU-versus-simplex
  figure is quarantined because the simplex baseline was unreliable; it is not
  a performance claim. Engine-matched measurements report GPU PDLP losing to
  CPU PDLP end-to-end on the measured instances.
- Conducted Nsight Systems profiling, occupancy, and roofline analysis; the
  historical profiling notes remain separate from the later RTX 2050 evidence.
- Refactored CLI solve app into modular cli_options and json_output components (<= 500 LOC).

### Phase 4: Sovereign Scaling & Audit Remediation (commit 23c8921)
- Implemented matrix-free first-order PDLP/PDHG solver with diagonal preconditioning.
- Implemented Mixed-Integer Rounding (MIR) cuts with cosine-similarity filtering.
- Implemented Strong Branching lookahead evaluation and domain reduction.
- Implemented multithreaded parallel tree search using C++20 std::jthread.
- Fixed Gomory cut generation by replacing slack discard with algebraic substitution.
- Added feasibility pump cycle prevention with ambiguous variable perturbation.
- Added CLI options: --engine pdlp|parallel, --threads, --branching rules.
- Synchronized capability documentation across README and CAPABILITY-MATRIX.

### Qualification Demo & Tooling Fixes (commit ec50874)
- Built markov-cero-info binary target and integrated with verification scripts.
- Updated qualification demo script and solver capability notice.

### Phase 3: Sovereign MILP Branch-and-Cut (commit 7dc74cd)
- Implemented sovereign branch-and-cut MILP solver with dual simplex node relaxations.
- Implemented node selection: best-bound, depth-first, and best-bound plunge.
- Implemented reliability pseudo-cost branching with most-fractional fallback.
- Implemented primal heuristics: simple rounding and feasibility pump.
- Implemented Gomory Mixed-Integer (GMI) cutting plane generator.
- Added MIPLIB benchmark runner and test suite (stein9, stein15, flugpl).
- Completed clean-room project rename to markov-cero across all sources.

## 0.5.1 — 2026-09-19

- Stopped tracking CMake `_m5-*` build trees; added `.gitignore`.
- Tightened `no_solver_guard.py` (forbids external solvers; allows clean-room lp).
- Reformatted M3/M4 simplex sources; renamed dual pricing `tableau_norm`.
- Added `markov-cero-solve`, refinery qualification models, and `scripts/run-qualification-demo.sh`.
- Documented current M5 capability vs planned M6–M11.

## 0.0.1 — 2026-09-13

- Established Apache-2.0 clean-room governance.
- Added canonical LP/QP, status/certificate, and numerical-policy specifications.
- Added threat, dependency-license, provenance, research, competitor,
  benchmark, evidence, and documentation schemas.
- Added CMake/CTest foundation and deterministic release verification/packaging scripts.
- Added no-solver guard and M0 acceptance report.

## 0.1.0 — 2026-09-13

- Added immutable model and CSC validation.
- Added strict MPS subset and diagnostics.
- Added independent primal/objective/integrality verifier.
- Added CLI, Python inspection path, tests, and fuzz target.

## 0.2.0 — 2026-09-13

- Added dense partial-pivoting LU, FTRAN/BTRAN, diagnostics, and residual checks.
- Added reversible LP canonicalization for objective sense, row senses,
  fixed/free/bounded variables, slacks, and postsolve.
- Added analytic and randomized M2 oracle tests.
