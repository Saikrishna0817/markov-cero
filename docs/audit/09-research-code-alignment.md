---
type: audit
phase: 6
title: Research ↔ Code Alignment Matrix
tags: [audit, alignment, phase-6, traceability]
status: complete
date: 2026-09-25
---

# Phase 6 — Research ↔ Code Alignment

Question answered here: *"we researched this" vs "we actually incorporated this"*.
Status legend: **GOOD** (present & evidenced) · **PARTIAL** · **GAP** · **REGRESSION**
(worse than baseline/nothing) · **UNPROVEN** (claim exists, evidence contradicts or is absent).

Research sources are cited as `[M##]` modules from `docs/research_paper_references.md`
(indexed notes in `docs/research/papers/`).

## 6.1 PS-requirement matrix (R1–R20)

| R# | Research requirement & source | Expected implementation | Current implementation (Observed) | Status | Action |
|---|---|---|---|---|---|
| R1 | Sovereign solver *core*, not modeling env — [M0] survey framing | Solver library + thin CLI/API | `markov_cero_core` static lib + 3 apps; no modeling layer built | **GOOD** | Keep; expose library API properly (AP-6) |
| R2 | LP + MILP + QP initial scope | 3 engine families | primal/dual/pdlp + branch-and-cut + ADMM QP/MIQP | **GOOD** | Keep |
| R3 | Modular → MIQP/NLP/MINLP later [M7, M15] | Engine interface + model abstraction | `--engine auto` dispatch; MIQP works via node LP; NLP/MINLP absent (allowed later) | **PARTIAL** | Keep shape; document extension points (R3 judged structurally) |
| R4 | **Revised simplex AND interior-point** [M3, M4: Karmarkar, Wright, Lustig, Ye-crossover] | IPM primal-dual interior point + crossover to basis | revised simplex ✅ (Bland) · **IPM: 0 hits** in src/include/apps · **crossover: 0 code hits** (only `scripts/plot_crossover.py` = CPU/GPU scale study, unrelated) | **GAP (hard)** | P0: implement or explicitly re-scope with evaluator-grade justification |
| R5 | B&B, B&C, cuts, presolve, heuristics, node selection [M7-M10] | cut loop inside tree, ≥4-6 cut classes, ≥2 heuristics, best-bound + DFS | B&B/B&C skeleton ✅ · cuts GMI+MIR **root-only** (node reduction 0.0%) · presolve **3 rule classes** · rounding+feasibility pump ✅ · best-bound heap ✅ (no DFS/dive) | **PARTIAL** | P0: re-cut inside tree; P1: presolve depth, diving |
| R6 | Sparse matrix techniques + numerical LA [M2: Markowitz, AMD, sparse LU] | CSC + sparse factorization + fill-reducing ordering | CSC ✅, sparse basis LU + eta ✅, **no Markowitz/AMD/fill-reducing ordering in factorization path** | **PARTIAL** | P1: pivoting/ordering quality work |
| R7 | Multi-core parallelization [M12: Huangfu, Berthold-ParSCIP] | thread pool with load balancing (work stealing) | `std::jthread` + atomics + best-bound queue; **no work stealing** (`rg -i steal src/milp/` → 0) → 4 threads **0.56×**, efficiency 14% | **REGRESSION** | P0: fix or disable claim; P1: work stealing |
| R8 | GPU where *measurable* benefit [M12: cuPDLP line] | per-scale measured GPU win with hardware reported | CUDA PDHG ✅ but `crossover_study.csv` GPU loses 13/13 end-to-end; 1 NumericalFailure passes CI (`run_gpu.py:241-245` bug) | **UNPROVEN** | P0: fix status check; re-benchmark; report hardware; narrow claim |
| R9 | Numerical stability / scalability / convergence emphasis [M11] | iterative refinement, condition estimation, tolerances policy | tolerances centralized (decision note) ✅ · **no iterative refinement, no condition estimation** (0 hits) | **PARTIAL** | P1: add both (they enable R17 demo) |
| R10 | From scratch, no OSS solver library [M0] | Clean-room + provenance + CI guard | `PROVENANCE.md`, `sovereignty_guard` CTest, `third_party/` empty of solver code, `readelf` NEEDED allowlist | **GOOD** | Keep; publish in demo |
| R11 | Industrial scope (refinery/blending/planning) [M14] | Case-study models in scope | `examples/refinery/*.mps`, refinery demo test; PS says *scope*, not required case studies | **PARTIAL** | P2: add 2-3 real-literature case models for the demo |
| R12 | Thousands→millions of vars/cons, sparse [M1, M13] | Sparse-first pipeline, tested at scale | dense gate 2048×8192 then sparse; largest Netlib extended run ~5.6 s; no 1e5+ row instance in evidence | **PARTIAL** | P1: large-scale test set (200k-1M) |
| R13 | Degeneracy + ill-conditioning robustness [M11: Bartels, Maros, Fourer, Neumaier] | Anti-degeneracy tactics, hard-instance suite | Bland ✅ (safe but slow), Harris ✅ (`dual_simplex.cpp:209`), **no steepest-edge** (header admits `dual_simplex.hpp:12`), no KKT-MIX/repair | **PARTIAL** | P1: steepest-edge + hard-instance dossier |
| R14 | API or CLI sufficient; no GUI | CLI + library API | 3 CLIs ✅; library API = headers only, no `install()`, orchestration stuck in app (AP-6) | **PARTIAL** | P1: install target + documented C++ API |
| R15 | MIPLIB / Netlib / Mittelmann benchmarks [M13] | ≥3 benchmark suites reported | Netlib 7 + extended 5 ✅, MIPLIB 3 ✅ (thin), **Mittelmann: 0 instances** | **PARTIAL** | P1: add Mittelmann sets (LP+MILP) |
| R16 | **Compare vs ≥1 established solver** [M13: Dolan–Moré, Mittelmann] | Same instance set, same hardware, both solvers, time/quality table + profiles | **Zero comparison artifacts repo-wide** (`rg -i "highs\|cplex\|gurobi\|cbc\|scip"` over evidence/reports/benchmarks → 0 hits) | **GAP (hard)** | **P0 #1**: HiGHS comparison harness (allowed: comparing ≠ building upon) |
| R17 | Numerical-robustness demonstration [M11] | Curated degenerate/ill-conditioned/weak-relaxation report | Degeneracy/ill-conditioning *unit tests* exist; **no demonstration dossier**, no ill-conditioned instance set | **GAP** | P0: build dossier (can reuse existing tests + curated instances) |
| R18 | Transparent, extensible, sovereign foundation [M0] | Docs + CI + provenance | Apache-2.0, CI 8 jobs, sovereignty guard; doc drift (AP-11) | **PARTIAL** | P2: doc sync |
| R19 | Datasets: MIPLIB, Netlib, Mittelmann, QPLIB + case studies [M13, M14] | Dataset ingestion + provenance | Netlib+MIPLIB data in `data/`; **QPLIB: absent**; case studies = 1 demo | **PARTIAL** | P2: QPLIB subset if QP story needs it |
| R20 | Optimal/near-optimal at practical times [M13] | Geometric-mean results tables | Single-solver results only (no baseline), geometric mean not computed vs anyone | **UNPROVEN** | Folded into R16 action |

## 6.2 Research knowledge NOT implemented (gap list, beyond PS wording)

| Gap | Sources | Why it matters | Priority |
|---|---|---|---|
| Interior-point + crossover | [M4] Karmarkar, Wright'97, Lustig, Ye'98 | R4 explicit; IPM is also the standard answer for large sparse LP | P0/P1 |
| Steepest-edge dual pricing | [M3B] Goldfarb'77/'92, Fourer'94, Koberstein'05 | dual simplex runs at *every* MIP node; without it node LPs are slow | P1 |
| Cut regeneration inside tree + cut pool | [M8] Cornuejols, Achterberg | current 0.0% node reduction | P0 |
| Work stealing / load balancing | [M12] Huangfu'18, Berthold'19 | current 0.56× parallel | P0 |
| Iterative refinement + condition estimation | [M11] | enables R17 robustness demo cheaply | P1 |
| Markowitz/AMD ordering in factorization | [M2] Markowitz'57, Amestoy'96, Davis'04 | fill/time of basis LU | P2 |
| Dolan–Moré performance profiles + geom. means | [M13] Dolan'02, Mittelmann | R16 comparison presentation format | P0 |
| Presolve depth (singleton/doubleton/probing/IM bound) | [M5] Achterberg'20, Savelsbergh'94 | current 3 rules | P1 |
| Diving / RINS-style local search | [M9, M16] | incumbent quality in hard MIPLIB instances | P2 |
| Cover/clique/odds cuts | [M8] | beyond GMI/MIR | P2 |

## 6.3 Code with NO research/problem justification

| Code | Observation | Verdict candidate |
|---|---|---|
| `benchmarks/runners/*.py` | md5-identical copies of `scripts/*.py` | REMOVE (dup) |
| `tests/fuzz/mps_coverage_fuzz.cpp` | not referenced by CMake | REMOVE or wire up |
| Dense canonicalization gate (≤2048×8192) | Inference: convenience threshold, no research basis, creates two paths | REWRITE to sparse-first |
| `PdlpOptions::power_iterations` (unused), `milp::Options::node_strategy` (never read), `PdlpStatus::infeasible_or_unbounded` (never produced) | dead options/status (component notes) | REMOVE |
| "29320× GPU speedup" headline | not corroborated by `crossover_study.csv` (13/13 GPU loses) | REMOVE claim |
| ML-assisted branching (Phase 7 plan, README:43) | [M16] research exists but no data pipeline, no trained model, 5 days to deadline | DEFER (explicitly out of scope) |

## 6.4 Research ideas technically unsuitable *for this project now*

- ML branching / learning-to-branch [M16]: needs training corpora + eval harness → beyond SIH window (Recommendation: defer; mention as roadmap P3).
- Full NLP/MINLP [M15]: PS says *later*; DOIs-to-derivative machinery is a project of its own.
- Asynchronous/PGAS parallel MIP [M12 Helbecque]: complexity ≫ benefit at 8-thread scale.
- Dense IPM (Karmarkar-era) for industrial sparse models: research itself moved to sparse Mehrotra-style; use sparse symmetric IPM only.

## 6.5 Existing implementation STRONGER than the research baseline

- Independent zero-trust verifiers (dual-gated canonical+original space): beyond typical open-source practice; keep as differentiation.
- Clean-room provenance + CI sovereignty guard: stronger auditability than most solver repos; directly serves R10/R18.
- Deterministic two-stage GPU reductions (bit-identical reruns): stronger determinism story than many GPU LP codes (Evidence: gpu determinism tests in Phase 5 commits) — worth *keeping and demonstrating*.

## 6.6 Missing experiments required to validate the system

| # | Experiment | Validates | Blocked by |
|---|---|---|---|
| E1 | Head-to-head vs HiGHS on shared instance set (time + quality, geometric mean, Dolan–Moré profile) | R16, R20 | harness (none exists) |
| E2 | Mittelmann-style LP/MILP suite run | R15 | instance download + harness |
| E3 | Hard-instance dossier: degenerate (Klee–Minty-style, Netlib degenerate), ill-conditioned (Bartels-type), weak-relaxation MI | R17, R13 | instance curation + iterative refinement |
| E4 | GPU vs CPU per-scale with recorded hardware + fix `run_gpu.py` status check | R8 | bug fix |
| E5 | Parallel scaling curve 1/2/4/8 threads after work-stealing fix | R7 | AP-4 fix |
| E6 | Cut-node-reduction after in-tree cut loop | R5 | AP-3 fix |

## 6.7 Verdict

Of 20 PS requirements: **4 GOOD, 10 PARTIAL, 3 GAP (R4, R16, R17), 1 REGRESSION (R7),
2 UNPROVEN (R8, R20)**. The research corpus has been *collected but not metabolized*:
202 paper notes now exist, yet the code contains no research-driven capability beyond what a
standard implementation would have — and the two hard gaps (R4 IPM, R16 comparison) are both
explicitly graded by the PS text.
