---

> Remediation note: this is the pre-fix audit baseline. Current changes and open gates are recorded in INDUSTRY-READINESS-IMPLEMENTATION-PLAN.md; historical defect descriptions below must not be read as current closure status.
type: audit
title: Current Codebase Audit — markov-cero
status: audit-complete-static-review
date: 2026-09-28
---

# Current Codebase Audit — 2026-09-28

## Scope and method

This report assesses the current checkout at `/home/saikrishna/markov-initial-build`, including its uncommitted working-tree changes. It covers the C++20 solver core and public headers, CLI/API, CUDA wrappers and kernels, Python binding, build and CI configuration, tests, scripts, benchmark evidence, and repository state. The review was static: files and diffs were inspected, and `git diff --check` was run. No build, CTest, fuzz, sanitizer, or live GPU workload was run, so runtime correctness is not independently certified here.

The repository is unusually broad for a solver prototype: LP (primal/dual simplex, IPM, PDLP), MILP/MIQP branch-and-cut, convex QP/ADMM, NLP/SQP and a constrained convex MINLP path, parsing (MPS/LP/NLOBJ), presolve/scaling, verification, CPU/GPU kernels, CLI/API, and pybind11. The existing test inventory is broad (57 files under `tests/` and `gpu/tests/`; CMake declares a large CTest suite). Evidence and current capability statements are recorded in `STATUS.md` and the 2026-09-28 benchmark folders.

## Executive assessment

The codebase has a credible modular solver architecture and has made substantial progress since the older `FINAL-AUDIT-REPORT.md` dated 2026-09-25. That report is stale as a statement of present capability: it says IPM, comparison, in-tree cuts, and API extraction are absent, while current source and `STATUS.md` describe those as implemented. The newer status register is more careful: benchmark coverage and comparison remain partial, GPU speed benefit is unproven, and R20 competitiveness remains unproven.

The most actionable issue found in this review is CI/test-environment wiring. The full CI CTest run invokes tests that require local Python virtual environments which CI never creates or populates. The CUDA job can also report success without compiling any CUDA code. These make the repository's stated CI gates unreliable until addressed.

**Post-review addendum (2026-09-28):** Source inspection and a read-only HiGHS parse also found a release-blocking MPS domain mismatch: after INTORG, explicit LO/LI lower bounds leave the parser's implicit upper bound at 1, while HiGHS reads bundled `gen-ip002` and `gen-ip054` integer columns as unbounded above. This narrows the parser finding to bound overrides; the marker-only 0–1 default itself follows major MPS conventions. See [F-26](sih_2026_findings.md) and the [implementation plan](INDUSTRY-READINESS-IMPLEMENTATION-PLAN.md). The original static review did not include this check.

## Findings

### F-01 — Full CTest suite requires absent comparison and Python environments

**Severity: High (CI blocker / reproducibility).**

`CMakeLists.txt` registers `compare_harness` with `${CMAKE_SOURCE_DIR}/.compare-venv/bin/python` and registers `python_bindings` with `${CMAKE_SOURCE_DIR}/.venv/bin/python` (lines 332–338, 370–376). `.compare-venv/` is explicitly gitignored. The GitHub workflow checks out the repository, configures, builds, and runs full CTest, but does not create either venv, install `highspy`, install `pybind11`, build the extension, or create `.venv` (`.github/workflows/ci.yml`, lines 45–67). `run_compare.py` exits nonzero when `highspy` cannot be imported (lines 264–283). `python_bindings` is marked skip only for exit code 77, but no CMake setup shown here arranges for the configured interpreter to exist or return that code when absent.

**Impact:** On a clean CI checkout the full CTest gate is expected to fail at those environment-dependent cases, regardless of core compilation. On a developer machine, results depend on ignored local environment state. This is a static configuration finding; this audit did not execute CTest.

**Recommendation:** Make these dependencies explicit and reproducible. Prefer a dedicated CMake option for integration tests, or have CI create/install the environments and build the Python extension before CTest. If optional, make the test itself return the configured skip code when prerequisites are missing, rather than pointing CTest at a nonexistent interpreter.

### F-02 — CUDA CI job accepts a CPU-only configuration as success

**Severity: High (validation gap).**

The CUDA workflow's toolkit install is allowed to fail (`.github/workflows/ci.yml`, lines 97–101). CMake explicitly disables `MARKOV_CERO_ENABLE_CUDA` when no compiler is found (`CMakeLists.txt`, lines 16–35). The workflow only prints the cache state with `|| true` and then builds and runs GPU-named tests (workflow lines 103–120); it never asserts that CUDA remained enabled or that CUDA translation units were compiled.

**Impact:** The job can be green while testing only CPU fallback, despite being named `cuda-build (compile + fallback)`. It therefore does not establish CUDA compilation coverage and should not be counted as closure of that risk.

**Recommendation:** Use a known CUDA toolkit installation action/container or a pinned runner image, fail if the CUDA compiler is unavailable, and assert the configured feature flag before build. Keep a separate CPU-only fallback job if needed.

### F-03 — Committed CI matrix does not test CUDA or ML enabled builds in a guaranteed way

**Severity: Medium (configuration coverage).**

CUDA is an optional feature (`MARKOV_CERO_ENABLE_CUDA`, default OFF) and ML branching is also optional (`MARKOV_CERO_ENABLE_ML`, default OFF) in `CMakeLists.txt`. The ordinary gcc/clang matrix does not enable either. The added CUDA job has the non-enforcing behavior described in F-02. The checked-in CI does not configure an ML-enabled build either.

**Impact:** Optional compilation paths can drift while ordinary CI remains green. This is especially material for CUDA, given its multiple `.cu` sources and the separate GPU QP path. ML is described in `STATUS.md` as experimental with acceptance still open, so it is lower priority.

**Recommendation:** Add at least one deterministic compile-only CI job per supported optional feature, with an explicit job result that distinguishes “compiled with feature” from CPU/no-feature fallback. ML can remain non-blocking until its runtime artifact and acceptance gate are ready.

### F-04 — Installer exports headers and binaries but no consumable CMake package metadata

**Severity: Medium (library usability).**

`CMakeLists.txt` installs the static library, executables, public headers, and provenance documents, but does not export a CMake target, generate a package config/version file, or install a pkg-config file. Consumers therefore cannot use a normal `find_package(markov_cero CONFIG)` workflow and must reconstruct link dependencies and include paths themselves. The install block is in lines 141–148.

**Impact:** The public C++ API is technically installed but downstream integration is fragile, particularly because the core also needs Threads and may have CUDA-related transitive requirements.

**Recommendation:** Export a namespaced target and package config with transitive dependencies, and add a clean-prefix consumer smoke check. Treat the Python wheel as a separate distribution path with its own documented build requirements.

### F-05 — Benchmark results support capability claims but not competitive-performance claims

**Severity: Medium (product/evaluation limitation).**

The current `STATUS.md` is commendably explicit: R15 remains partial, R16 remains partial, and R20 is unproven. It records only 51/98 Netlib, 4/119 MIPLIB, 4/20 Mittelmann, and 4/18 QPLIB optimal outcomes at a 15-second solver-side cap, and a 20-case repeated HiGHS comparison with runtime ratios favoring HiGHS. GPU runs show correctness on measured examples but no end-to-end speed benefit. The broad experiment is useful as a transparent baseline, but current coverage and time caps are inadequate to claim broad industrial robustness or competitive speed.

**Impact:** Public positioning must continue to distinguish implemented engines from validated capability and from demonstrated performance. The README's “fully implemented and independently verified” wording should be read as feature-level, not a claim of broad benchmark success.

**Recommendation:** Preserve per-binary hashes and source revision, run larger pre-registered suites under matched timing boundaries, publish failures/timeouts as first-class outcomes, and keep R15/R16/R20 qualified until coverage improves. Do not use GPU acceleration as a performance claim absent a representative crossover.

### F-06 — Working tree is not a clean audit/release baseline

**Severity: Medium (change-control and reproducibility).**

`git status` shows broad modifications across build configuration, CI, docs, solver implementations, tests, evidence, and scripts; it also reports generated build trees, large local benchmark/data additions, session artifacts, and new documentation. The status output exceeds 600 entries. The changes are not scoped to one coherent patch in the working tree, and current binaries in build directories may not correspond to current source.

**Impact:** Reviewers cannot infer which changes are intentional, what evidence was produced from this exact source, or whether checked-in/generated data are appropriate for release. Stale binaries can produce misleading validation results.

**Recommendation:** Before release, inventory and explicitly classify every untracked path, remove only confirmed generated artifacts, preserve meaningful benchmark provenance, and create a clean source revision plus a manifest mapping evidence to source and binary hashes. Do not clean or discard this shared working tree without explicit direction.

### F-07 — `git diff --check` reports trailing whitespace in changed benchmark CSVs

**Severity: Low (hygiene).**

`git diff --check` reports trailing whitespace on every added line in `evidence/benchmarks/crossover_study.csv`, `evidence/miplib_results.csv`, and `evidence/netlib_results.csv`. The cells appear to have trailing spaces after CSV values.

**Impact:** This creates noisy diffs and indicates the CSVs lack a normalization check. It does not by itself change CSV interpretation for ordinary readers.

**Recommendation:** Strip trailing whitespace and add a lightweight data-format validation to the evidence generation/review workflow.

## Architecture and subsystem review

| Area | Assessment | Main caveat |
|---|---|---|
| Model and sparse storage | Good foundation: immutable model API and CSC-oriented storage; sparse canonicalization now appears to be the main production path. | Parser and dimension validation remain high-risk due to hostile/large input handling; targeted fuzzing and allocation-bound review remain important. |
| LP engines | Broad implementation: primal/dual revised simplex, PDLP, Mehrotra IPM and crossover. Strong separation between engine results and independent verification. | Broad algorithm coverage is not equivalent to robust solve coverage; current suite outcomes show substantial hard-instance failures/timeouts. |
| MILP/MIQP | Includes branch-and-cut, cuts, heuristics, strong/reliability branching, and parallel work distribution. | `STATUS.md` reports large-root LP and deadline behavior still open; ML branching remains experimental and has no accepted durable artifact/data gate. |
| QP | ADMM, sparse LDLᵀ, convexity checks, and KKT verification are a strong coherent feature set. | GPU QP path offloads only part of the work per status notes; must not be described as full GPU KKT/ADMM acceleration. |
| NLP/MINLP | SQP/L-BFGS, verifier, quadratic nonlinear format, and convex OA subset make the scope substantially broader than the older audit. | Convexity and functional scope are restricted; NLP optimality and infeasibility guarantees differ from LP certificates. Public docs should keep the restrictions prominent. |
| CUDA | GPU code has explicit fallback and timing telemetry, plus GPU test sources. | Actual CUDA CI enablement is not enforced; physical GPU evidence is local and current status says GPU loses on measured cases. |
| API and CLI | A library-level solve API and thin CLI orchestration are a meaningful structural improvement. JSON diagnostics and statuses appear central to the interface. | Install lacks package metadata (F-04); API compatibility/versioning and exception/error contracts need a documented guarantee before external adoption. |
| Python | pybind11 binding and tests exist. | Build path is coupled to an already-built static CMake archive and default `build_w5`; CTest/CI do not establish a clean reproducible wheel/build path. |
| Tests | Large unit, property, integration, and benchmark suite; assert checks are explicitly retained for test targets in Release. | CI wiring defects (F-01/F-02) weaken the meaning of a nominally green pipeline. No tests were executed in this audit. |
| Data/evidence | Many source datasets have provenance sidecars; current status document contains hashes and qualified outcome tables. | Large dataset/evidence additions in this working tree need size/licensing/provenance review and strict source-binary linkage. |

## Positive controls and strengths

- Clean-room provenance is an explicit design goal, with a sovereignty guard in the CMake test suite.
- LP/QP/NLP verification code is structurally separate from the solvers and results are fail-closed in key paths.
- The code is split into domain modules rather than one solver translation unit; the CLI orchestration was extracted into `src/api/`.
- The status/evidence register openly records poor benchmark outcomes, fallback behavior, GPU regressions, and incomplete ML acceptance rather than hiding them.
- The CMake configuration exposes sanitizers, fuzzing, CUDA, and ML as explicit options.
- The test suite includes property tests for sparse basis, warm starts, presolve, model behavior, and end-to-end feature/boundary/scenario coverage.

## Risk-ranked action plan

1. **Fix CI prerequisites and skip semantics (F-01).** Make a clean checkout able to configure, build, and run the intended default test set deterministically.
2. **Make CUDA CI truth-preserving (F-02/F-03).** Establish a guaranteed toolkit environment or report the job as skipped, never as a successful CUDA compile when it fell back.
3. **Freeze and identify a source baseline.** Classify the 600+ changed/untracked entries and bind benchmark evidence to source and binary hashes (F-06).
4. **Continue honest evaluation.** Expand suites and align timing boundaries before making breadth or speed claims (F-05).
5. **Finish distribution surface.** Export a proper CMake package and validate a consumer build (F-04).
6. **Normalize evidence files.** Resolve the whitespace and add a small CSV/schema validation gate (F-07).

## Audit conclusion

The architecture and implementation breadth are strong for a research/prototype solver, and the current state is materially ahead of the prior September 25 audit. The primary release risk is not a single obvious solver defect established by this static pass; it is confidence in build/test and evidence provenance. Fix CI dependency wiring, make optional CUDA coverage enforceable, and establish a clean reproducible source/evidence baseline before treating passing automation or benchmark records as release certification.
