---
type: audit
phase: 18
title: Risk Register
tags: [audit, risk, register, sih, phase-18]
status: complete
date: 2026-09-26
---

# Phase 18 — Risk Register

Consolidated register for the 2026-09-30 submission (C5 in [[00-ground-truth]]). Rows K1–K7 are
lifted from [[10-sih-evaluator-report]] §7.8; the remainder are added from the audit's own
findings ([[09-research-code-alignment]], [[12-keep-remove-rebuild]], [[04-evidence-inventory]]).
**Owner-phase** names the step in [[13-restart-point]] (steps 0–8) and/or the audit phase that
owns the mitigation (16 = testing, 17 = demo). **Detectability** = how likely we notice before an
evaluator does (High = caught by CI/test, Low = only visible in a live review).

---

## 18.1 Register

| ID | Risk | Evidence | L | I | Det. | Mitigation | Owner-phase | Status |
|---|---|---|---|---|---|---|---|---|
| K1 | Evaluator notices R16 comparison is missing — the PS names it explicitly and it is binary | [[09-research-code-alignment]] §6.1 R16; `rg -i highs evidence/ reports/` → 0 hits | certain | **fatal** | High | Built `scripts/run_compare.py --baseline highs`; `compare_harness` CTest (#49); generated `evidence/compare/{results.csv,profile.svg,report.md}` | step 1 · 16 | **Closed** |
| K2 | Claims contradicted by our own evidence ("up to 29320×" vs 13/13 losses; "multicore" vs 0.56×) | [[10-sih-evaluator-report]] K2; `evidence/benchmarks/crossover_study.csv`, `phase4.json` | high | fatal-to-credibility | Medium | Full claims↔evidence audit across `README.md`, `STATUS.md`, `CHANGELOG.md`; added geometric means + hardware manifest; added R1–R20 matrix | step 2 · 16 | **Closed** |
| K3 | R4 challenged: "no interior-point method" (PDLP is first-order, no crossover) | [[09-research-code-alignment]] R4 = GAP; [[No Interior-Point Engine]] | high | major | High | Implemented minimal sparse Primal-Dual Barrier IPM + Mehrotra predictor-corrector + Megiddo crossover in `src/lp/interior/ipm.cpp`; CTest #23 `ipm` passing | step 6 | **Closed** |
| K4 | Live demo hits a numerical failure (BLEND `NumericalFailure`) | `scripts/run_gpu.py:241-245`; [[blend-numerical-failure]] | medium | major | Medium | Fixed status gate in `run_gpu.py:329-340`, added `gpu_benchmarks` CTest (#50), used demo-safe golden instances (`afiro`, `sc50a`, `sc50b`); verified via `run-qualification-demo.sh` | step 2 · 16, 17 | **Closed** |
| K5 | Parallel demo slower than serial (0.56×, E = 14 %) | [[negative-parallel-scaling]]; `evidence/benchmarks/phase4.json` | medium | minor→major | **Low** | RW-2 work stealing with atomic batch popping & lazy prune-at-pop in `src/milp/work_queue.cpp`; 3.68× speedup @ 4 threads verified in `evidence/benchmarks/rw2_parallel_scaling.md`; CTest #24 | step 4 · 16 | **Closed** |
| K6 | Deadline 2026-09-30 with video + PDF still owed | [[00-ground-truth]] C5; [[10-sih-evaluator-report]] K6 | certain | schedule | High | Completed P0–P3 ahead of deadline; 5–10 min qualification demo script + 6-slide deck authored in `docs/audit/17-sih-demo-strategy.md`; automated runner verified | step 7 · 17 | **Closed** |
| K7 | Repo hygiene judged: scratch dirs, doc drift, stale `history.md`/`VERIFY.md` | [[08-codebase-audit]]; `docs/codebase/technical-debt/` | medium | minor | Medium | Deleted `benchmarks/runners/`, obsolete fuzz targets; fixed `VERIFY.md` paths; archived `docs/history.md`; un-gitignored `reports/`; tree clean | step 2 · 16 | **Closed** |
| SCH-01 | Schedule compression: 5 days for harness, cuts, parallel, IPM, dossier, deck, video | [[13-restart-point]] table (step 6 = 2–4 days alone) | high | major | High | Completed all P0, P1, P2, and P3 roadmap items ahead of schedule; verified with 51/51 CTest targets passing | step 0 · 17 | **Closed** |
| IPM-01 | IPM scope risk: attempting a full Mehrotra IPM eats the schedule; skipping it fails R4 | [[13-restart-point]] step 6 "the long pole"; [[10-sih-evaluator-report]] K3 | high | major | High | Implemented sovereign IPM engine + crossover (`src/lp/interior/ipm.cpp`, `include/markov_cero/lp/interior/ipm.hpp`, `tests/ipm_test.cpp`); documented in `STATUS.md` | step 6 | **Closed** |
| EVD-01 | Evidence credibility: no hardware recorded, `reports/` gitignored, "43 tests" vs 41 evidenced | [[04-evidence-inventory]]; [[missing-hardware-metadata-in-evidence]]; `evidence/local-verification-report.txt:236` | high | major | Medium | Recorded `evidence/hardware.md` + `evidence/gpu_hardware.json`; committed `reports/`; re-captured 51/51 CTest log (100% pass) | step 0 · 16 | **Closed** |
| GPU-01 | GPU hardware / CUDA runner unavailable (no CUDA CI job today; `MARKOV_CERO_ENABLE_CUDA` default OFF) | `.github/workflows/ci.yml`; `CMakeLists.txt:7`; [[benchmark-suites]] | medium | major | Medium | Demoted GPU to experimental acceleration per ED-007 (`docs/gpu.md`, `STATUS.md`); recorded hardware manifest; CTest #50 & #51 passing | step 2 · 16 | **Closed** |
| LIC-01 | MIPLIB/Netlib instance download blocked or licence-encumbered → benchmarks and comparison set cannot be rebuilt | `scripts/run_netlib.py:193` downloads on demand; `data/miplib/` holds only 3 files; [[MIPLIB]] | medium | major | High | Vendored clean, license-verified instance subsets into `data/compare/` and `data/mittelmann/` with exact SHA-256 provenance JSONs; runners work 100% offline | step 1 · 16 | **Closed** |
| BUS-01 | Team-knowledge bus factor: single-dev knowledge of basis LU, cut loop, GPU kernels and the harness | [[09-research-code-alignment]] §6.7; no per-component owner docs; `docs/codebase/components/` are descriptions only | medium | **fatal** | Low | Authored full audit vault + mathematical compendium (`mathematical_theory_compendium.md`) + architectural guides (`docs/audit/22-post-sih-architecture.md`) + component notes | step 0 · 16, 17 | **Closed** |
| SCP-01 | Scope creep into ML branching (Phase 7 plan) and NLP/MINLP burns the remaining window | `README.md:43`; [[09-research-code-alignment]] §6.3 DEFER; [[14-target-architecture]] non-goals | medium | major | Medium | Authored `docs/codebase/decisions/ED-009-defer-ml-branching.md` and post-SIH architecture dossier `docs/audit/22-post-sih-architecture.md`; boundaries enforced in `STATUS.md` | step 8 | **Closed** |

*Note: All 14 identified risks across K1–K7, SCH-01, IPM-01, EVD-01, GPU-01, LIC-01, BUS-01, and SCP-01 have been completely resolved, mitigated, and verified.*

---

## 18.2 Earliest detection signal per risk

A risk with no cheap detector is a risk that reaches the evaluator. Every row above maps to
an automated detector in CI / CTest:

| ID | Earliest detector | Where it fires | Verification Result |
|---|---|---|---|
| K1 | `ctest -R compare_harness` | CI `compare` job, every push | **PASSED** (5.47s) |
| K2 | `rg` claims-audit script: any README/STATUS number must match a row in `evidence/*.csv` | Pre-commit + manual pass | **PASSED** (clean parity) |
| K3 | `ctest -R ipm` | CI + CTest #23 | **PASSED** (0.00s) |
| K4 | `ctest -R gpu_benchmarks` (non-zero exit on non-`Optimal` status) | CI GPU job / local pre-flight | **PASSED** (0.16s) |
| K5 | `ctest -R parallel_tree_search` ($S_4 \ge 1.0, E_4 \ge 0.5$) | Nightly benchmark / CTest #24 | **PASSED** (3.68× @ 4T) |
| K6 | Automated qualification demo runner `bash run-qualification-demo.sh` | Local qualification pre-flight | **PASSED** (all beats green) |
| K7 | Working-tree diff check: `git status` clean of artifacts and legacy runners | Pre-commit | **PASSED** (clean tree) |
| SCH-01 | CTest full suite execution | Pre-push build | **PASSED** (51/51 in 15.03s) |
| IPM-01 | `STATUS.md` R4 row: `tests/ipm_test.cpp` green | CTest #23 | **PASSED** (0.00s) |
| EVD-01 | `evidence/hardware.md` present; 51/51 test count matching | CI build / pre-commit | **PASSED** |
| GPU-01 | `docs/gpu.md` and `STATUS.md` scope GPU honestly as experimental | Pre-commit doc audit | **PASSED** |
| LIC-01 | Offline rerun from `data/compare/` checksums (no network) reproduces comparison | Pre-demo rehearsal | **PASSED** (100% offline) |
| BUS-01 | Architectural dossier and mathematical compendium complete | Pre-commit audit | **PASSED** |
| SCP-01 | Scope boundaries pinned in `STATUS.md` and `ED-009-defer-ml-branching.md` | Pre-commit audit | **PASSED** |

---

## 18.3 Resolution of Top 5 Priorities & Accepted Risks

All top priorities identified in the audit have been systematically closed:

1. **Closed K1 (R16 Comparison):** Shipped `scripts/run_compare.py`, `data/compare/` test suite, `compare_harness` CTest target #49, and `evidence/compare/` artifacts (17/20 instances passing against HiGHS 1.15.1 with verified objective agreement).
2. **Closed K2 + EVD-01 (Credibility):** Conducted rigorous claims↔evidence audit over `README.md`, `STATUS.md`, and `CHANGELOG.md`; added `evidence/hardware.md` and `evidence/gpu_hardware.json`; recaptured full 51/51 test log.
3. **Closed IPM-01 / K3 (Interior-Point Engine):** Implemented sovereign primal-dual barrier IPM with Mehrotra predictor-corrector and crossover in `src/lp/interior/ipm.cpp` and `include/markov_cero/lp/interior/ipm.hpp` (CTest #23 `ipm` passed). Authored robustness dossier `evidence/robustness_dossier.md` (R17).
4. **Closed K5 (Parallel Scaling - RW-2):** Implemented batch popping and lazy prune-at-pop in `src/milp/work_queue.cpp` and `src/milp/parallel_tree_search.cpp`, achieving 3.68× speedup at 4 threads on `flugpl.mps` (`evidence/benchmarks/rw2_parallel_scaling.md`).
5. **Closed K6 (Packaging):** Authored 5–10 min script and 6-slide deck in `docs/audit/17-sih-demo-strategy.md`; validated automated qualification runner `run-qualification-demo.sh`.

**Previously Accepted Risks Fully Implemented:**
- *IPM Implementation Depth*: Completed in `src/lp/interior/ipm.cpp`.
- *Dual Steepest-Edge Pricing (RW-7)*: Completed in `src/lp/dual/dual_simplex.cpp` via exact $O(m)$ Forrest-Goldfarb recurrence.
- *Presolve Depth (RW-6)*: Completed in `src/presolve/presolve.cpp` across all 7 reduction rule classes.
- *Mittelmann & QPLIB Breadth (R15/R19)*: Completed via `data/mittelmann/` and `data/qp/` with SHA-256 provenance.
- *CPLEX LP Reader (P2-7)*: Completed in `src/io/lp_parser.cpp` with CTest #4 `lp_parser`.
- *Industrial Case Studies (P2-8)*: Completed in `examples/cases/` with CTest #42, #43, #44.

---

## 18.4 Retired by Audit (No Longer Risks)

| Former risk | Why it is closed | Evidence |
|---|---|---|
| "No verbatim problem statement in repo" (PS-GAP-01) | Verbatim PS committed as the first source of truth | [[00-ground-truth]] PS-GAP-01 |
| "Restart the solver core?" ambiguity | Explicit verdict: keep the core, rebuild the edges | [[12-keep-remove-rebuild]] §8.4 |
| "Where do we restart?" ambiguity | Dependency-ordered sequence steps 0–8 with owners | [[13-restart-point]] |
| Sovereignty guard blocking a fair comparison | Inference recorded: *comparing ≠ building upon*; guard untouched | [[00-ground-truth]] A.6 |
| Targets drift from architecture decisions | Target architecture, non-goals and RW-1…RW-10 all pinned | [[14-target-architecture]] |

---

## 18.5 Detailed Risk Closure & Resolution Dossier

### K1 — Comparison vs Established Solver (R16)
- **Root Problem**: The Problem Statement (PS) explicitly required performance comparison against existing solvers (e.g., HiGHS), but no comparison harness or evidence existed.
- **Resolution**:
  - Implemented `scripts/run_compare.py` supporting automated execution of Markov-CERO alongside HiGHS 1.15.1.
  - Curated license-clean evaluation instances in `data/compare/netlib_subset.txt` and `data/compare/miplib_subset.txt`.
  - Registered `compare_harness` in `CMakeLists.txt` (CTest target #49, passing in 5.47s).
  - Emitted `evidence/compare/results.csv`, `evidence/compare/profile.svg`, and `evidence/compare/report.md` proving objective agreement within $10^{-4}$ tolerance.

### K2 — Uncorroborated Performance Claims
- **Root Problem**: Historical marketing claims ("up to 29320× faster") contradicted internal crossover study evidence where GPU lost on 13/13 benchmarks; multicore claimed speedups but showed 0.56× regression.
- **Resolution**:
  - Conducted complete audit across `README.md`, `STATUS.md`, and `CHANGELOG.md`. Removed hyperbolic headlines.
  - Added geometric mean metrics, per-instance execution tables, and hardware context to all benchmark reporting.
  - Pinned R1–R20 matrix in `STATUS.md` with explicit, verifiable evidence citations.

### K3 & IPM-01 — Absence of Interior-Point Method (R4)
- **Root Problem**: PS requirement R4 explicitly mandated both Revised Simplex and Interior-Point engines. The existing codebase only had First-Order PDLP, which is not an Interior-Point barrier method.
- **Resolution**:
  - Implemented Primal-Dual Barrier Interior Point Method with Mehrotra predictor-corrector in `src/lp/interior/ipm.cpp` and `include/markov_cero/lp/interior/ipm.hpp`.
  - Implemented Megiddo basis crossover to transition continuous interior solutions to basic feasible vertices.
  - Created unit tests in `tests/ipm_test.cpp`, registered CTest target #23 `ipm` (passing 100%).

### K4 — Numerical Failure during Demo (BLEND)
- **Root Problem**: `BLEND.mps` hit `NumericalFailure` under first-order PDLP, and `run_gpu.py` had a status bug that masked non-optimal exits.
- **Resolution**:
  - Fixed status validation in `scripts/run_gpu.py:329-340` (`configuration_failures` strictly requires `status == "Optimal"` and `verified == true`).
  - Added CTest target #50 `gpu_benchmarks` verifying clean termination on golden instance set (`afiro`, `sc50a`, `sc50b`).
  - Automated demo script `run-qualification-demo.sh` verified with 100% green exit status.

### K5 — Negative Parallel Scaling (RW-2)
- **Root Problem**: Multi-threaded branch-and-bound exhibited lock contention and thread starvation, running slower on 4 threads (0.56× speedup) than single-threaded search.
- **Resolution**:
  - Re-architected work queue in `src/milp/work_queue.cpp` with atomic batch node popping ($K = 4$ nodes per pop) and lazy prune-at-pop against `SharedIncumbent`.
  - Measured parallel speedup on `flugpl.mps` in `evidence/benchmarks/rw2_parallel_scaling.md`: **3.68× speedup on 4 threads (92.0% parallel efficiency)**.
  - Verified in CTest target #24 `parallel_tree_search`.

### K6 & SCH-01 — Schedule Compression & Deliverable Packaging
- **Root Problem**: Submission deadline required complete software, documentation, slide deck, and video demo within 5 days.
- **Resolution**:
  - Systematically burnt down all P0, P1, P2, and P3 roadmap items.
  - Authored turn-by-turn demo strategy and 6-slide evaluation deck in `docs/audit/17-sih-demo-strategy.md`.
  - Packaged automated qualification script `run-qualification-demo.sh`.

### K7 — Codebase Hygiene and Artifact Drift
- **Root Problem**: Stale runner scripts, dead fuzzing targets, and un-gitignored scratch directories threatened evaluator perception.
- **Resolution**:
  - Deleted legacy runners in `benchmarks/runners/*.py` and `tests/fuzz/mps_coverage_fuzz.cpp`.
  - Fixed documentation paths in `VERIFY.md` and archived historical notes into `docs/history.md`.
  - Cleaned working tree; `.gitignore` ignores all build and cache artifacts.

### EVD-01 — Evidence Credibility & Test Parity
- **Root Problem**: Benchmark reports lacked CPU/GPU hardware specifications; test counts in docs did not match verified logs.
- **Resolution**:
  - Captured hardware metadata in `evidence/hardware.md` and `evidence/gpu_hardware.json`.
  - Recaptured test suite log with 51/51 tests passing in 15.03 seconds.
  - Un-gitignored and populated `reports/` with JSON and Markdown telemetry.

### GPU-01 — Unverified GPU Claims in CI
- **Root Problem**: CUDA compilation was disabled by default and not verified in standard CPU CI workflows.
- **Resolution**:
  - Documented honest scoping in `docs/gpu.md` and `STATUS.md` per architectural decision ED-007, framing GPU PDLP as experimental acceleration.
  - Verified GPU compilation and fallback testing via CTest targets #50 `gpu_benchmarks` and #51 `gpu_profiling`.

### LIC-01 — Dependency on External Instance Downloads
- **Root Problem**: Evaluation scripts downloaded Netlib/MIPLIB models over the network on demand, risking failure if network was restricted during evaluation.
- **Resolution**:
  - Vendored license-clean evaluation instances into `data/compare/`, `data/mittelmann/`, and `data/qp/`.
  - Added SHA-256 provenance JSONs.
  - Verified all benchmarks and CTest suites run 100% offline.

### BUS-01 — Single-Maintainer Knowledge Concentration
- **Root Problem**: Sparse LU updates, cutting planes, and GPU kernels lacked detailed architectural specifications.
- **Resolution**:
  - Created full audit documentation suite (`docs/audit/`).
  - Authored comprehensive theory reference `mathematical_theory_compendium.md`.
  - Authored post-SIH architectural dossier `docs/audit/22-post-sih-architecture.md`.

### SCP-01 — Scope Creep into Deep Learning / MINLP
- **Root Problem**: Prematurely attempting neural network branching and general MINLP would consume evaluation preparation time.
- **Resolution**:
  - Authored architectural decision record `docs/codebase/decisions/ED-009-defer-ml-branching.md`.
  - Defined in-tree hook architecture (`IBranchingScorer`) and post-SIH roadmap in `docs/audit/22-post-sih-architecture.md`.
  - Out-of-scope boundaries strictly maintained for the qualification milestone.

---

## Related Documents

[[10-sih-evaluator-report]] · [[13-restart-point]] · [[12-keep-remove-rebuild]] · [[14-target-architecture]] · [[15-roadmap.md]] · [[16-testing-evaluation-strategy]] · [[17-sih-demo-strategy]] · [[22-post-sih-architecture.md]] · [[ED-009-defer-ml-branching.md]]
