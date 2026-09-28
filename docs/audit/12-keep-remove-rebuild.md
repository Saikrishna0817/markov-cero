---
type: audit
phase: 8
title: KEEP / REMOVE / REBUILD Decision Plan
tags: [audit, keep-remove-rebuild, phase-8]
status: complete
date: 2026-09-25
---

# Phase 8 — Keep / Remove / Rebuild

Evidence keys: [A]=PS (`docs/sih26119_problem_statement.md`), [B]=research (`docs/research/`),
[C]=code (`docs/audit/08-codebase-audit.md`, `docs/audit/07-current-architecture.md`),
[D]=SIH (`docs/audit/10-sih-evaluator-report.md`).

## 8.1 KEEP — valuable, evidenced, low-need of change

| Component | Why valuable | Evidence | Dependencies | Required improvement |
|---|---|---|---|---|
| Clean-room provenance + `sovereignty_guard` + `third_party/` emptiness | Only *provable* R10 story; evaluator-grade differentiator | [A] R10/R18, [D] 7.2 | CI | none (publish in demo) |
| Independent verifiers (canonical+original dual-gated, KKT certificate) | Trust layer beyond typical OSS; enables "certified sovereign" narrative | [C] `src/verify/*`, [B] trust practice | core model | wire certificates into demo output |
| Immutable CSC model + LIFO presolve/postsolve stack | Correct canonical architecture | [B] [M5], [C] | — | none structurally |
| Revised simplex core (Phase-I/II, sparse basis LU, eta updates) | R4 simplex half; foundation for warm starts | [A] R4, [B] [M3A] | linalg | raise caps to config; keep Bland as fallback |
| Dual simplex + Harris | Node engine + R4/B4 backbone | [C] `dual_simplex.cpp:209` | basis | add steepest-edge (REBUILD item below) |
| PDLP/PDHG CPU engine + Ruiz scaling | Research-backed first-order engine, big-instance path | [B] [M12] | — | keep |
| Branch-and-cut skeleton: branching strategies, queue, heuristics | R5 skeleton correct | [C] | LP engines | cut-loop rewiring (REBUILD) |
| QP/MIQP: ADMM + Davis LDLᵀ + PSD detection + KKT verifier | R2 QP fully met; strongest area | [C] `src/qp/*`, [D] | — | needs real QP data (`data/qp` empty) |
| MPS parser, examples/refinery suite, JSON output | demo + test assets | [C] | — | parser limitations (P2) |
| Netlib/MIPLIB runners + 43 ctest targets + 8-job CI | R15 harness exists | [C] | — | add comparison runner (P0) |
| Evidence folder + plotting scripts | audit trail of performance claims | [C] | — | add hardware metadata; un-gitignore `reports/` |
| GPU PDHG engine (code itself) | R8 *if* benefits proven; deterministic reductions are strong | [C] `gpu/*` | CUDA | re-benchmark honestly; fix status bug |
| Vault (`docs/research/`, `docs/codebase/`, `docs/audit/`) | knowledge asset for repo + evaluator | this audit | — | maintain via `link_backlinks.py` |

## 8.2 REMOVE — creates cost, no value

| Item | Why unnecessary | Problem it creates | Deps? | Replacement |
|---|---|---|---|---|
| `benchmarks/runners/*.py` (2 files) | md5-identical to `scripts/` | dual maintenance, confusion | none | `scripts/*` |
| `tests/fuzz/mps_coverage_fuzz.cpp` + `generate-fuzz-corpus.py` (if fuzzing stays unwired) | not in CMake; never runs | dead code illusion | none | optionally re-wire (P2 INVESTIGATE) |
| Dead options/status: `PdlpOptions::power_iterations`, `milp::Options::node_strategy`, `PdlpStatus::infeasible_or_unbounded` | never read/produced | API lies; evaluator finds them | none | delete |
| Root scratch dirs `_m5-*`, `_deployment-*`, `_verify-*`, `build/` | local artifacts (gitignored) | working-tree noise; local-only evidence duplicated | none | rebuild on demand |
| Uncorroborated claims: "up to 29320×", implied GPU dominance, implied parallel speedup | contradicted by own CSVs (13/13, 0.56×) | credibility risk **[D] K2** | none | honest claims table (P0) |
| `docs/history.md` as living doc (stops Phase 4) | stale | doc/code contradiction | none | archive + point to CHANGELOG |
| Empty dirs `data/qp` (without data), unused codebase graph stubs | noise | — | none | add data or delete |

## 8.3 REBUILD / REWRITE — right idea, wrong implementation

| # | Component | Current | Problems | Desired | Research justification | Priority |
|---|---|---|---|---|---|---|
| RW-1 | **Cut management in B&B** (`milp_solver.cpp` + cut_pool) | GMI+MIR at root only; node reduction 0.0% | never re-cut at children; pool unused → R5 unmet in practice | in-tree cut loop: generate at nodes with separation frequency, pool reuse, validity checks; measure cut-node reduction (target >20% on MIPLIB set) | [B] [M8] Cornuejols, Achterberg, Junger | **P0** |
| RW-2 | **Parallel tree search** (`parallel_tree_search.cpp`, `work_queue.cpp`) | jthread + atomics + shared best-bound heap, no stealing | 4 threads = 0.56× (14% eff) → R7 regression | load-balanced queue with work stealing + subtree limits (or default `--threads 1` until fixed); re-measure scaling curve | [B] [M12] Huangfu, Berthold-ParSCIP | **P0** |
| RW-3 | **Benchmark/comparison harness** (missing entirely) | `run_netlib.py`/`run_miplib.py` single-solver | R16 hard gap | `run_compare.py --baseline highs` producing shared-instance table: status, objective, time, gap; geometric means [M13]; emit markdown+CSV for slides | [B] [M13] Dolan–Moré, Mittelmann | **P0** |
| RW-4 | **App orchestration** (`apps/markov_cero_solve.cpp` 499 LOC) | dispatch+verify+timing+output in main | no real library API (R14 partial), untestable | move orchestration to `src/api/` (`solve_file()`, `solve_model()`), CLI becomes thin | [A] R14, [B] architecture practice | **P1** |
| RW-5 | **Canonicalization paths** (dense gate + sparse) | two divergent paths, threshold 2048×8192 | untested large-scale path; R12 risk | single sparse-first path; dense kept only for tiny models via same interface | [B] [M1] | **P1** |
| RW-6 | **Presolve rule set** (3 classes) | empty row/col, singleton | vs research baseline of dozens; R5/R13 weak | add: implied bounds, forcing/dominated rows, duplicate rows, probing-lite; keep LIFO postsolve | [B] [M5] Achterberg 2020, Savelsbergh 1994 | **P1** |
| RW-7 | **Dual simplex pricing** | Harris + non-steepest-edge `tableau_norm` (admitted) | slow node LPs; degeneracy handling weak | dual steepest-edge with density-limited norms; keep Bland fallback | [B] [M3B] Goldfarb, Koberstein | **P1** |
| RW-8 | **Numerical robustness tooling** | none (no refinement, no condition estimator) | R17 demo has no engine support | iterative refinement on LU solves + simple condition estimation; feed a "robustness dossier" report | [B] [M11] | **P0** (dossier) / P1 (tooling) |
| RW-9 | **GPU evaluation + claim surface** | losing 13/13, BLEND failure hidden, no hardware info | R8 unproven; credibility risk | fix `run_gpu.py:241-245`; record hardware; per-scale re-run; scope claims to honest crossover (or demote GPU to appendix until it wins) | [A] R8, [B] [M12] | **P0** |
| RW-10 | **Docs/claims layer** (README/STATUS/CHANGELOG/VERIFY/history) | drift vs evidence | evaluator trap | claims-evidence table; fix VERIFY paths; extend STATUS with R1–R20 coverage table | [D] K2 | **P1** |

## 8.4 START FROM SCRATCH — explicit verdict

**Do NOT restart the solver core.** The foundation (model, sparse basis, engines, verifiers,
QP, provenance) is sound, tested, and worth more than 5 days of rewriting — restarting
violates operating rule 10 and would guarantee missing the Sep-30 deadline.

**Do NOT keep the *evaluation layer* — because it barely exists.** What exists of it
(single-solver CSVs, claims without baselines, hidden numerical failure) must be replaced,
not preserved. The comparison harness, robustness dossier and claim audit are effectively
greenfield work.

**Do NOT preserve the parallel-search design** — it measures as negative value; it is
re-architected (RW-2) rather than patched.

> Net: *keep the core, rebuild the edges* (cuts loop, parallel scheduling, evaluation,
> API surface, numerical tooling). See `docs/audit/13-restart-point.md` for exact order.
