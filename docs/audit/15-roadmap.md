---
type: audit
phase: 14
title: Implementation Roadmap (P0–P3)
tags: [audit, roadmap, phase-14, priorities]
status: complete
date: 2026-09-25
---

# Phase 14 — Prioritized Implementation Roadmap

Deadline context: idea submission **2026-09-30** (5 days), then regional rounds with PPT/video/demo.
Priority: **P0 critical** (submission fails or evaluator fails us without it) · **P1 important**
(grade-defining) · **P2 valuable** (polish) · **P3 optional** (post-SIH).
Dependencies follow `docs/audit/13-restart-point.md`.

---

## P0 — Critical (must complete before 2026-09-30)

### P0-1 Comparison harness (R16) — *the single most important item*
- **Objective:** produce an honest, reproducible head-to-head table vs ≥1 established solver on shared instances.
- **Components/files:** new `scripts/run_compare.py` (modeled on existing `scripts/run_netlib.py`); new `data/compare/` instance list; CI step in `.github/workflows/ci.yml`; output `evidence/compare/{results.csv,profile.png,report.md}`.
- **Dependencies:** step 0 hardware manifest (P0-2). Uses HiGHS **as external binary only** — `scripts/check-sovereignty.py` unchanged (comparing ≠ building upon; R10 intact).
- **Research basis:** [[Dolan-2002-Benchmarking-Optimization-Software]] ([[Geometric Mean Runtime]], Dolan–Moré profiles) [M13]; [[Mittelmann-n.d.-Benchmarks-Optimization-Software]].
- **Deliverables:** results table (status, objective, time, relative gap per instance), geometric means, profile plot, `reports/` copy committed.
- **Tests/DoD:** `run_compare.py --instances netlib,miplib --baseline highs` exits 0, produces ≥15 shared instances with both solvers' statuses; markdown table appears in evidence; DoD = evaluator can reproduce in one command.
- **Risks:** HiGHS not installable on demo machine (mitigate: apt/wheel pinned in script, fallback = CBC/GLPK; risk K in `18-risk-register.md`).

### P0-2 Claims audit + hardware manifest + benchmark bug fix (R8/R7/R18, K2)
- **Objective:** make every public claim match evidence.
- **Files:** `README.md` (GPU crossover claim), `STATUS.md` (add R1–R20 coverage table), `CHANGELOG.md` (41 vs 43 test counts), `docs/gpu.md`, `scripts/run_gpu.py:241-245` (**fix: check `res_simplex["status"]` before recording success**), new `evidence/hardware.md` (CPU/GPU/RAM/OS/compiler).
- **Research basis:** [[Dolan-2002-Benchmarking-Optimization-Software]] (reporting methodology).
- **Tests/DoD:** `run_gpu.py` on BLEND exits non-zero / records FAIL; every number in README traceable to a file under `evidence/`; `git grep "29320"` → 0 hits.
- **Risks:** admitting GPU loses invites questions — mitigated by honest crossover framing (slide 5 in `17-sih-demo-strategy.md`).

### P0-3 In-tree cut loop (R5, RW-1)
- **Objective:** cuts re-separated inside branch-and-bound; cut-node reduction > 0.
- **Files:** `src/milp/milp_solver.cpp` (cut application at child nodes with frequency), `src/milp/cut_pool.cpp` (reuse + validity), tests `tests/cut_separation_test.cpp` (new).
- **Research basis:** [[Cornuejols-2001-Branch-and-Cut-Algorithms]], [[Achterberg-2005-General-Mixed-Integer]], [[Gomory Mixed Integer Cut]], [[Mixed Integer Rounding Cut]], [[Cut Pooling]].
- **Deliverables:** `evidence/cut_effectiveness.csv` (nodes, cuts, before/after on 3 MIPLIB instances).
- **Tests/DoD:** node count drops ≥20% on ≥2 of 3 MIPLIB instances vs P0-1 baseline; existing `milp_test`/`strong_branching_test` still green; ctest full pass.
- **Risks:** worse performance on some instances (guard with per-instance limit + option `--cuts-root-only` retained as fallback).

### P0-4 Parallel scheduling fix or demotion (R7, RW-2)
- **Objective:** stop shipping a measured regression.
- **Files:** `src/milp/work_queue.cpp` + `parallel_tree_search.cpp` (work-stealing deque or subtree partitioning); or minimal viable: default `--threads 1` for MILP until fixed; tests `tests/parallel_tree_search_test.cpp` extended.
- **Research basis:** [[Huangfu-2018-Parallelizing-Dual-Revised]] style hybrid sync, [[Berthold-2019-Parallel-SCIP-UG]], [[Work Stealing]], [[Parallel Speedup]]/[[Parallel Efficiency]].
- **Tests/DoD:** scaling curve 1/2/4 threads speedup ≥1.5× at 4 threads on a ≥10k-node instance, OR claims/docs default to serial (honest state); evidence JSON updated with curve.
- **Risks:** real fix is 1-2 days — decide demote-vs-fix on day 1 (decision [[ED-006-load-balanced-parallel-or-demote]]).

### P0-5 Robustness dossier (R17/R13, E3)
- **Objective:** a demonstrable "numerical robustness" artifact as PS demands.
- **Files:** `data/robustness/{degenerate,illconditioned,weakrelaxation}/` curated instances (from Netlib degenerate + constructed Klee–Minty/Bartels-type + weakly relaxed MI), new `scripts/run_robustness.py`, output `evidence/robustness/report.md` + chart; optionally wire iterative refinement first if a naive baseline diverges.
- **Research basis:** [[Bartels-1969-Simplex-LU-Decomposition]], [[Maros-2003-Generalized-Dual-Phase]], [[Degeneracy]], [[Ill-Conditioning]], [[Iterative Refinement]], [[Neumaier-2004-Safe-Bounds-Linear]].
- **Tests/DoD:** ≥9 instances (3 per class), all solved with certificates or explained failure modes; report referenced by demo script; command one-liner.

### P0-6 Demo packaging + deliverables (SIH rules)
- **Objective:** 6-slide PDF, narrated non-AI video, working `run-qualification-demo.sh` v2.
- **Files:** `run-qualification-demo.sh` (wire P0-1/2/5 outputs), `reports/demo/*`, deck + video (outside repo or under `reports/demo/`).
- **DoD:** script runs offline end-to-end on a fresh clone; video ≤10 min with live team narration; deck maps 1:1 to `17-sih-demo-strategy.md` outline.

---

## P1 — Important (grade-defining; first week after submission / parallel if staffed)

### P1-1 Interior-point engine + crossover decision (R4)
- **Objective:** close the second hard gap, or formally re-scope.
- **Option A (implement):** minimal sparse Mehrotra predictor-corrector for LP (`src/lp/ipm/`), crossover via weighted-primal/dual start into the existing simplex basis machinery (reuse `src/linalg/sparse_basis.cpp`); CLI `--engine ipm`. Files: `src/lp/ipm/ipm_solver.cpp`, `include/.../lp/ipm/`, `tests/ipm_test.cpp`, Netlib subset comparison vs primal simplex.
- **Option B (re-scope):** evidence memo showing first-order PDLP meets R9 on large sparse LPs + IPM deferred; accepted only if Option A is infeasible in schedule — evaluator risk K3.
- **Research basis:** [[Karmarkar-1984-New-Polynomial-Time-Algorithm]], [[Wright-1997-Primal-Dual-Interior-Point-Methods]], [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]], [[Ye-1998-Crossover-Interior-Point]], [[Crossover]], [[Interior-Point Method]].
- **DoD:** IPM solves ≥6/7 core Netlib instances; crossover produces a basis certified by existing verifiers; README claim updated.

### P1-2 Dual steepest-edge pricing (R13, node performance)
- **Files:** `src/lp/dual/dual_simplex.cpp` + `dual_simplex.hpp:12` (replace pseudo `tableau_norm` with Goldberg–Goldfarb steepest-edge norms + guard), tests `tests/dual_steepest_test.cpp` (iter-count comparison on degenerate instances).
- **Research basis:** [[Goldfarb-1977-Practicable-Steepest-Edge]], [[Goldfarb-1992-Steepest-Edge-Simplex]], [[Fourer-1994-Steepest-Edge-Simplexing]], [[Koberstein-2005-Dual-Simplex-Method]], [[Steepest Edge]].
- **DoD:** LP iteration counts improve ≥15% on degenerate Netlib subset; no cycling (Bland retained as fallback).

### P1-3 Presolve depth (R5)
- **Files:** `src/presolve/presolve.cpp` (+ rule modules), keep LIFO postsolve; tests for each rule round-trip.
- **Research basis:** [[Achterberg-2020-Presolve-Reductions-Mixed]], [[Savelsbergh-1994-Preprocessing-Probing-Techniques]], [[Presolve]].
- **DoD:** ≥7 rule classes; reductions counted in stats; no postsolve mismatches (verifier pass rate 100% on Netlib+MIPLIB).

### P1-4 Library API surface (R14, RW-4)
- **Files:** new `src/api/solve.cpp` + `include/markov_cero/api.hpp` (`solve_file()`, `solve_model()` returning result struct with certificate), `apps/markov_cero_solve.cpp` slimmed to parse+call, CMake `install()` + exported target, `docs/codebase/APIs/Library-API.md` updated, one example `examples/api_demo.cpp`.
- **DoD:** external-style consumer compiles against installed package; app file <200 lines; DoD test in CI.

### P1-5 Sparse-first canonicalization (R12, RW-5)
- **Files:** merge `transform/canonicalize.cpp` + `sparse_canonicalize.cpp` behind one interface, dense path only as size-optimized branch; `tests/canonicalize_*` updated.
- **DoD:** single dispatch point; 100k-row instance canonicalizes + solves (new scale test); behavior parity tests pass.

### P1-6 Benchmark suite expansion (R15/R19, E2)
- **Files:** `data/mittelmann/` (LP + MILP sets), extend `scripts/run_netlib.py` → `run_suite.py`; `data/qp/` populated (QPLIB subset or hand-built convex QP set — directory currently empty).
- **DoD:** Mittelmann run recorded in evidence with geometric means; `data/qp` non-empty with ≥5 instances used by a test.

### P1-7 Doc/claims consolidation (R18)
- **Files:** `docs/history.md` (extend or archive), `VERIFY.md` (fix dangling `provenance/` paths), `STATUS.md` R1–R20 table, README capability section synced with evidence.
- **DoD:** no doc claim contradicts `evidence/` (spot-check checklist in `08-codebase-audit.md` §4.4 all resolved).

---

## P2 — Valuable

- **P2-1** Dead code & hygiene: delete `benchmarks/runners/*.py`, dead options (`power_iterations`, `node_strategy`, `infeasible_or_unbounded`), decide fuzz target (`tests/fuzz/mps_coverage_fuzz.cpp` — wire or remove), clean scratch dirs, restore/remove empty `data/qp`.
- **P2-2** Heuristics depth: diving heuristic + local-swap repair (`src/milp/heuristics.cpp`) — [[Rounding Heuristic]], [[Diving]], [[Canturk-2024-Scalable-Primal-Heuristics]].
- **P2-3** Cut family expansion: cover/odd-hole cuts ([[Cornuejols-2008-Valid-Inequalities-Mixed]]) behind options.
- **P2-4** Markowitz/AMD-quality pivoting + fill-reducing ordering in `sparse_basis.cpp` ([[Markowitz-1957-Elimination-Form-Inverse]], [[Amestoy-1996-Approximate-Minimum-Degree]], [[Fill-Reducing Ordering]]).
- **P2-5** Condition estimation + iterative refinement in LU path (feeds P0-5 robustly) ([[Iterative Refinement]], [[Ill-Conditioning]]).
- **P2-6** GPU CI job (CUDA runner or nightly self-hosted) or demote GPU claims to "experimental" ([[GPU CSR SpMV]]).
- **P2-7** MPS parser limitations (error messages, QUADOBJ coverage tests), LP-format reader implemented (`src/io/lp_parser.cpp`, `include/markov_cero/io/lp_parser.hpp`, `tests/lp_parser_test.cpp`).
- **P2-8** Industrial case-study models from open literature (refinery/blending/production planning) for narrative + R11 demo (`examples/cases/` + `examples/cases/README.md`, [[Neiro-2004-Mathematical-Modeling-Petroleum]], [[Kallrath-2002-Planning-Scheduling-Industry]]).

## P3 — Optional / post-SIH (Architectural Dossier: `docs/audit/22-post-sih-architecture.md`)

- ML-assisted branching [[Zhang-2025-Learning-Select-Nodes]] [[Kimiaei-2025-Machine-Learning-Algorithms]] — needs data pipeline first (decision [[ED-009-defer-ml-branching]]). Hook architecture specified in `22-post-sih-architecture.md` §2.
- MIQP tightening (perspective cuts [[Linan-2025-Trends-Perspectives-Deterministic]]), NLP/MINLP extension points (R3). Formalized in `22-post-sih-architecture.md` §3 & §4.
- Asynchronous/PGAS parallel MIP, GPU cut separation — only with measured benefit (R8 rule). Formalized in `22-post-sih-architecture.md` §5.
- Modeling-layer sugar (sets/parameters) — explicitly out of PS scope (R1). Formalized in `22-post-sih-architecture.md` §6.

---

## Dependency graph (P0 → P1)

```
P0-2 (manifest+fixes) ─┐
P0-1 (harness) ────────┼─► P0-3 (cuts)   ─► P1-2 (steepest-edge) ─► P2-4/2-5
                       ├─► P0-4 (parallel) ─► (re-measure via P0-1)
                       └─► P0-5 (dossier) ─► P1-3 (presolve) ─► P1-5
P0-1+P0-2 ─► P1-6 (suites) ─► E2 profiles ─► P1-1 (IPM, judged by where it wins)
P0-3..P0-5 ─► P0-6 (demo packaging)
(any) ─► P1-4 (API) ─► P2-8 (case studies)
```

## Schedule sketch (3-person team, to Sep 30)

| Day | Dev A (harness/eval) | Dev B (MILP core) | Dev C (proof/demo) |
|---|---|---|---|
| D1 | P0-1 harness + HiGHS install | P0-4 decide fix-vs-demote, start fix | P0-2 claims audit + hardware.md |
| D2 | P0-1 suites + geometric means | P0-3 cut loop | P0-5 robustness instances |
| D3 | P0-1 evidence outputs | P0-3 measurement + tests | P0-5 report + refinement |
| D4 | P0-4 verification runs | P0-4 scaling curve | P0-6 demo script + slides |
| D5 | buffer / reruns | buffer | video recording + final QA |

**Definition of done for the deadline:** all six P0 items evidenced in `evidence/` + `reports/`,
`bash run-qualification-demo.sh` green on fresh clone, deck + video produced, no claim in
README contradicted by evidence.
