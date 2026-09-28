---
type: audit
phase: 4
title: Codebase Forensic Audit — Inventory & Verdicts
tags: [audit, codebase, phase-4, keep-remove-rebuild]
status: complete
date: 2026-09-25
---

# Phase 4 — Codebase Forensic Audit

Scope: entire repository (src, include, gpu, apps, tests, benchmarks, scripts, data,
examples, docs, evidence, reports, CI, build config, scratch dirs). Every verdict is argued
against **[A] PS requirements**, **[B] research evidence**, **[C] observed code state**,
**[D] SIH demonstrability** — not code quality alone. Component-level detail:
`docs/codebase/components/*`; debt: `docs/codebase/technical-debt/*`.

Verdict key: **KEEP** · **MODIFY** (keep + change) · **REWRITE** (keep idea, replace impl) ·
**MOVE** · **DEPRECATE** (stop using, keep for record) · **REMOVE** · **INVESTIGATE**.

## 4.1 Directory-level inventory

| Path | What it is (Observed) | Used? | Tested? | PS-relevant? | Research-backed? | Verdict |
|---|---|---|---|---|---|---|
| `src/` (7,872 LOC) | core engine library `markov_cero_core` | yes (apps+tests) | yes (22 unit tests) | R1-R9 core | partially (see 09 matrix) | **KEEP** (with targeted REWRITEs in 4.2) |
| `include/markov_cero/*` | public headers, 11 sub-ns | yes | transitively | R14 API | — | **MODIFY**: add install()/API docs, remove dead options |
| `gpu/` (4,091 LOC) | CUDA PDHG engine + kernels + 8 tests | via `--backend gpu` | local only, **no CI CUDA job** | R8 | [M12] supports *if* beneficial | **MODIFY**: honest scoping (R8 unproven, 13/13 loss) |
| `apps/` (1,024 LOC) | 3 CLIs; `markov_cero_solve.cpp` 499-line monolith | yes | CLI ctests | R14 | — | **MODIFY**: extract orchestration into lib (AP-6) |
| `tests/` (22 files) + `gpu/tests/` (8) | unit/component + CLI + benchmark tests | ctest 43 | — | R15 partial | [M13] | **KEEP** + extend (testing gaps note) |
| `tests/fuzz/mps_coverage_fuzz.cpp` | fuzz target **not referenced by CMake** | no | no | parser robustness (R9) | — | **REMOVE** or wire up (INVESTIGATE only if fuzzing kept) |
| `benchmarks/runners/` | 2 files, **md5-identical** to `scripts/` | no (dup) | — | — | — | **REMOVE** (dup) |
| `scripts/` (12 py/sh) | netlib/miplib/gpu runners, sovereignty guard, plotting, release verify, **new** `link_backlinks.py` | yes | partially | R15/R16/R8 | [M13] | **KEEP**; `run_gpu.py` **MODIFY** (status-check bug :241-245); add comparison harness (P0) |
| `data/netlib` (744K) | Netlib LP instances | yes | via netlib ctest | R15 | [M13] Netlib | **KEEP** |
| `data/miplib` (20K) | 3 MIPLIB instances | yes | thin | R15 | [M13] MIPLIB | **MODIFY**: expand set |
| `data/qp` | **empty directory (0 bytes)** | — | — | R2/QPLIB (R19) | [M6][M13] | **MODIFY**: add QPLIB-style QP instances or delete |
| `data/scale_study` (6.9M) | synthetic scale instances | yes (local) | gitignored? | R12 | — | **KEEP** (verify git status) |
| `examples/` | toy blend, qp portfolio, refinery suite (feasible/infeasible/limited/malformed + expected JSON) | yes (demo, CLI tests) | yes | R11, demo | [M14] | **KEEP** — demo assets |
| `evidence/` + `reports/` | benchmark CSVs/JSONs/NSight analysis; `reports/`+`data/scale_study` gitignored (local-only) | archive | — | R15-R17 | [M13] | **MODIFY**: add hardware metadata; publish results |
| `docs/` | architecture/contracts/gpu/math/references/history/decisions/ADR/governance + new vault | partly | — | R18 | — | **MODIFY**: fix drift (history stops Phase 4; VERIFY.md dangling paths); vault is KEEP |
| `spec/security/` | security spec docs | read-only | — | hygiene | — | **KEEP** |
| `third_party/` | only LICENSES/README — **no solver code** | — | — | R10 | — | **KEEP** (evidence of R10) |
| `.github/workflows/ci.yml` | 8 jobs gcc/clang × Debug/Release/ASan/TSan + ctest+netlib+miplib+demo | yes | — | R10/R15 | — | **MODIFY**: add comparison + CUDA-or-drop-GPU-claims |
| Root scratch dirs (`_m5-*`, `_deployment-*`, `_verify-*`, `build/`) | local build trees, **gitignored** | local | — | — | — | **REMOVE from working tree** (hygiene; gitignore already correct) |
| `run-qualification-demo.sh`, `VERIFY.md` | judge demo + verification doc | yes | — | demo | — | **KEEP**; **MODIFY** VERIFY.md (references non-existent `provenance/`) |
| `PROVENANCE.md`, `NOTICE`, `LICENSE` | clean-room provenance | CI (guard) | yes | **R10** | — | **KEEP** (differentiator) |
| `README.md`, `QUICKSTART.md`, `STATUS.md`, `CHANGELOG.md` | claims + onboarding | — | — | R18 | — | **MODIFY**: claims vs evidence drift (0.56×, 13/13 GPU loss, 41 vs 43 tests) |

## 4.2 Source-module verdicts (src/gpu)

| Module | Verdict | Why (A/B/C/D) |
|---|---|---|
| `lp/reference/revised_simplex.cpp` (497) | **KEEP** + MOD caps | R4 simplex half ✅; research [M3A]; caps 1024×8192/1e6 iter must be config (R12) |
| `lp/dual/dual_simplex.cpp` (499) | **KEEP** + MODIFY | Harris ✅; missing steepest-edge [M3B] is the single biggest node-speed lever (research finding → decision) |
| `lp/first_order/pdlp.cpp` (419) | **KEEP** | first-order engine; complements (not replaces) IPM (R4) |
| `linalg/sparse_basis.cpp` (463) | **KEEP** + MODIFY | correct substrate; add Markowitz/AMD-quality pivoting [M2] |
| `linalg/dense_lu.cpp` (194) | **KEEP** (small-scale fallback) | fine for small nodes/tests |
| `presolve/presolve.cpp` (319) | **REWRITE** (extend) | 3 rules vs research baseline dozens [M5]; idea (LIFO postsolve) correct → keep stack, add rules |
| `scale/ruiz_scaling.cpp` (260) | **KEEP** | research-backed [M5/M1] |
| `transform/canonicalize.cpp` (199) dense + `sparse_canonicalize.cpp` (329) | **REWRITE** to sparse-first | two paths, arbitrary dense gate (AP-7); research: sparse is the industrial reality [M1] |
| `io/mps.cpp` (456) | **KEEP** + MODIFY | R19 formats; parser limitations note; needs LP-format later? (P3) |
| `milp/milp_solver.cpp` (478) | **MODIFY** | cut loop only at root → AP-3; node strategy option unread (dead option) |
| `milp/gomory.cpp`, `mir.cpp`, `cut_pool.cpp` | **KEEP** + MODIFY | implementations OK; wiring wrong (root-only); pool underused |
| `milp/strong_branching.cpp`, `branch_selector.cpp` | **KEEP** | [M10] research-backed |
| `milp/heuristics.cpp` (305) | **KEEP** + MODIFY | rounding+FP ✅ [M9]; add diving later (P2) |
| `milp/work_queue.cpp`, `parallel_tree_search.cpp`, `shared_incumbent.cpp` | **REWRITE** (load balancing) | no work stealing → 0.56× (R7 REGRESSION); research [M12] prescribes stealing/hybrid sync |
| `qp/*` (model, admm_solver, kkt, verifier) | **KEEP** | R2 QP ✅; strongest area alongside verifiers |
| `verify/*` | **KEEP** (differentiator) | trust story beyond baseline (6.5) |
| `gpu/*` | **MODIFY** | fix `run_gpu.py` status bug; per-scale re-bench with hardware; CI CUDA job or demote claims |
| `apps/markov_cero_solve.cpp` (499) | **REWRITE** (extract) | dispatch/verify/timing/output monolith → library orchestration so R14 "API" is real |
| dead options: `PdlpOptions::power_iterations`, `milp::Options::node_strategy`, `PdlpStatus::infeasible_or_unbounded` | **REMOVE** | dead code (verified in component notes) |

## 4.3 Hidden functionality check (before any removal)

- `cut_pool.cpp`: used only by root cut phase — removing would break B&C tests; **keep** (needs more wiring, not removal).
- `dual` engine: reachable only via `--engine dual` or internal LP calls at MIP nodes — **not dead**, despite CLI demos rarely using it.
- `DenseLU`: referenced by small-model LP paths — **not dead**.
- `examples/refinery/*malformed.mps`: used by negative CLI tests — **not dead**.
- `scripts/generate-fuzz-corpus.py`: supports the *unwired* fuzz target — decide together with `mps_coverage_fuzz.cpp` (INVESTIGATE pair).
- `docs/threat-model.md` (5 lines) + `spec/security/`: thin but referenced by security doc index — KEEP.

## 4.4 Documentation vs code discrepancies (Observed → action)

| Claim | Reality | Action |
|---|---|---|
| README "GPU … verified scale crossover study" | crossover_study.csv: GPU loses 13/13 end-to-end | rewrite claim (P0 doc fix) |
| README "Harris two-pass" | true (`dual_simplex.hpp:30`) | keep |
| CHANGELOG "43 targets 100% pass" | local evidence showed 41/41 at one point; **re-verified in this audit: fresh RelWithDebInfo build + `ctest` = 43/43 passed (9.5 s)** | claim is now substantiated — keep |
| `docs/history.md` ends Phase 4 | Phase 5/6 exist | extend or archive (P2) |
| `VERIFY.md` → `provenance/` | dir doesn't exist | fix paths (P2) |
| Phase-6 complete claim | true for QP code; PS-coverage still 4 GOOD/10 PARTIAL/3 GAP… (see 09) | add PS-coverage table to STATUS.md (P1) |

## 4.5 Security posture (Observed)

- No secrets in repo (guard + license allowlist in CI); `third_party/` empty of code;
  input parser is the attack surface (fuzz target exists but is unwired → **INVESTIGATE/wire**).
- No network calls at runtime; benchmark dataset fetch is offline-tracked for Netlib/MIPLIB.
- Verdict: **KEEP** posture; wire fuzzing (P2), no new security work required for SIH scope.

## 4.6 Counts

- **KEEP** 18 groups · **MODIFY** 17 · **REWRITE** 5 · **REMOVE** 3 · **INVESTIGATE** 1 pair ·
  **DEPRECATE** 0. (Detailed per-component mapping in `docs/audit/12-keep-remove-rebuild.md`.)
