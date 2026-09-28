---
type: audit
phase: 5
title: Architecture Reconstruction (Current System)
tags: [audit, architecture, phase-5]
status: complete
date: 2026-09-25
---

# Phase 5 — Architecture Reconstruction (As Built)

All component facts verified against source during this audit; component-level notes live in
`docs/codebase/components/` and are linked inline. Fact vs inference: bullets starting
"Inference:" are deductions.

## C.1 Pipeline (actual data flow)

```
MPS file  (src/io/mps.cpp — free-format parser, QUADOBJ/QMATRIX read)
   │  Model (immutable, CSC columns)
   ▼
canonicalize / sparse_canonicalize  (src/transform/*.cpp)
   │  dense path ≤2048×8192 else sparse CSC
   ▼
presolve (3 rule classes: empty row / empty col / row-singleton→fixed)
   │  LIFO postsolve stack  +  Ruiz scaling (src/scale/ruiz_scaling.cpp)
   ▼
engine dispatch — apps/markov_cero_solve.cpp:64-312  (--engine auto|primal|dual|pdlp|milp|qp|miqp|parallel)
   │
   ├─ primal : revised Phase-I/II simplex, Bland anti-cycling (src/lp/reference/revised_simplex.cpp)
   ├─ dual   : dual revised simplex, warm start, Harris ratio test (src/lp/dual/dual_simplex.cpp:209)
   ├─ pdlp   : CPU PDHG (src/lp/first_order/pdlp.cpp) ── backend=gpu → gpu::solve_pdlp_gpu
   │            (gpu/src/pdhg_step.cpp:357, device-resident loop, GPU CSR SpMV kernels)
   ├─ milp   : branch-and-cut (src/milp/milp_solver.cpp)
   │            ├─ cuts: GMI + MIR, **generated at root only** (src/milp/{gomory,mir,cut_pool}.cpp)
   │            ├─ branching: most-fractional / strong / reliability pseudo-cost (branch_selector.cpp)
   │            ├─ heuristics: 3 rounding strategies + feasibility pump (heuristics.cpp)
   │            └─ node queue: best-bound heap (work_queue.cpp:19), no work stealing
   ├─ parallel: std::jthread workers, atomics counters (parallel_tree_search.cpp:460)
   ├─ qp     : ADMM over quasi-definite KKT (src/qp/*), Davis LDLᵀ (src/qp/kkt.cpp, "Algorithm 849")
   └─ miqp   : milp_solver with QP relaxations via src/milp/node_lp.cpp
   ▼
postsolve (scale reversal + bound/constraint restoration)
   ▼
Independent verifiers: reference_lp_verifier / primal_verifier / KKT certificate verifier
   ▼
JSON solution (apps/json_output.hpp) + console summary
```

Library boundary: static lib `markov_cero_core` (CMake) + three apps
(`markov-cero-solve`, `-info`, `-mps-inspect`). Public headers under `include/markov_cero/`
(foundation, io, linalg, lp, milp, model, presolve, qp, scale, transform, verify).

## C.2 Component → dependency map (verified)

| Component | Depends on | Note |
|---|---|---|
| [[RevisedSimplexEngine]] | [[SparseBasis-LU]], [[Canonicalizer]] | caps 1024×8192, 1e6 iters |
| [[DualSimplexEngine]] | [[SparseBasis-LU]] | Harris on (default true, `dual_simplex.hpp:30`); **not** steepest edge — header comment at `dual_simplex.hpp:12` admits its `tableau_norm` "is not conventional dual steepest-edge" |
| [[PDLP-Engine]] | [[RuizScaling]], [[GPU-PDHG-Engine]] | first-order PDHG, diagonal preconditioning |
| [[Presolve]] | model, LIFO postsolve | 3 rule classes |
| [[BranchAndCut]] | [[CutGenerators]], [[PrimalHeuristics]], [[RevisedSimplexEngine]]/node LP | root-only cuts |
| [[QP-ADMM-Engine]] | [[LDL-Factorization]], [[IndependentVerifiers]] | |
| all engines | [[MPSParser]], [[Canonicalizer]], [[Solution-JSON-Writer]] | |

Full data-flow note: `docs/codebase/data-flow/Solve-Pipeline.md`.

## C.3 Quality/verification infrastructure (as built)

- 43 `add_test` in root `CMakeLists.txt:166-221`: 22 C++ unit sources (`tests/*.cpp`) + 8 GPU
  tests, CLI tests, benchmark tests (`netlib_benchmarks`, `miplib_benchmarks`,
  `gpu_benchmarks`, `gpu_profiling`), and `sovereignty_guard` running
  `scripts/check-sovereignty.py` (CMake allowlist + include scan + `readelf -d` NEEDED allowlist).
- CI: 8 jobs gcc/clang × Debug/Release/ASan-UBSan/TSan, warnings-as-errors,
  ctest + netlib + miplib + refinery demo. **No CUDA job in CI** (GPU code is CI-untested).
- Evidence pipeline: `scripts/run_netlib.py`, `run_gpu.py`, `run_miplib.py`,
  `benchmarks/runners/*` (md5-identical duplicates of scripts/).

## C.4 Architectural problems (Phase 5 findings)

Each is later classified KEEP/REMOVE/REBUILD in `docs/audit/12-keep-remove-rebuild.md`.

| # | Problem | Evidence | Class |
|---|---|---|---|
| AP-1 | **No interior-point engine and no crossover** despite PS R4 requiring "revised simplex **and** interior-point methods"; every search for `interior.point|ipm|predictor-corrector` in src/include/apps returns 0 hits | PS R4; `rg` empty | Missing component |
| AP-2 | **Evaluation subsystem is missing entirely** — no external-solver harness, no Mittelmann set, no run-comparison tooling; PS R16 binary requirement | `rg -i "highs\|cplex\|gurobi\|cbc\|scip"` over evidence/reports/benchmarks → 0 hits | Missing component |
| AP-3 | **Cuts are root-only** → cut-node reduction 0.0%; B&C degenerates into bounded best-bound B&B | `evidence/benchmarks/phase4.json:39,49`; no re-cut call site in `milp_solver.cpp` | Under-built component |
| AP-4 | **Parallel search has no work stealing / no load balancing** → 4 threads = 0.56× speedup (slowdown), 14% efficiency | `phase4.json:60-61`; `rg -i steal src/milp/` → 0 hits | Defective component |
| AP-5 | **GPU loses at every measured scale** (13/13 end-to-end) yet is a headline claim; `BLEND → NumericalFailure` passes CI because `run_gpu.py:241-245` never checks solver status | `evidence/benchmarks/crossover_study.csv`; `_deployment-phase2-build/gpu_benchmark.csv:3` | Mis-evaluated component + test bug |
| AP-6 | **Monolithic `apps/markov_cero_solve.cpp` (~500 lines) owns dispatch, verification, timing, output** — engines are library code but orchestration isn't; hard to reuse as an "API" (PS R14) | `apps/markov_cero_solve.cpp:64-494` | Coupling / wrong abstraction |
| AP-7 | **Dense-first canonicalization gate** (dense ≤2048×8192) means R12 "thousands to millions of variables" routing depends on one threshold; sparse path is a separate code path with different tested behavior | `src/transform/canonicalize.cpp`, `sparse_canonicalize.cpp` | Inference: two code paths to keep in sync |
| AP-8 | **No numerical robustness tooling**: no iterative refinement, no condition estimation (`rg "iterative.refinement\|condition.est"` → 0 hits) — PS R17 demo has no engine support | verified | Missing component |
| AP-9 | Presolve is 3 rule classes where research baseline (Achterberg 2020, Savelsbergh 1994) implies dozens | `src/presolve/presolve.cpp:67-151` | Under-built |
| AP-10 | GPU path not covered by CI; determinism claims tested locally only | `.github/workflows/ci.yml` (no CUDA job) | Testing gap |
| AP-11 | `docs/history.md` stops at Phase 4 while CHANGELOG/README claim Phase 6; doc/code drift is structural, not incidental | `docs/history.md` | Technical debt |
| AP-12 | Dead/duplicate artifacts: `tests/fuzz/mps_coverage_fuzz.cpp` (unreferenced), `benchmarks/runners/*.py` (md5 dupes), 15+ scratch dirs in working tree | verified | Hygiene |

## C.5 What is architecturally *right*

- Clean-room + sovereignty guard is enforced by CI (`sovereignty_guard`) — PS R10 is provable.
- Independent zero-trust verifiers (dual-gated) match research best practice for trust in a new solver.
- Immutable model + CSC core + reversible presolve/postsolve stack = the correct canonical
  architecture ([[CSC Sparse Model]], [[Presolve-Postsolve Stack]]).
- Multi-engine dispatch with `--engine auto` is the right extensibility shape for R3.
- Sparse-basis LU/eta substrate under simplex is the right foundation for warm starts (R5).

**Inference:** the *skeleton* is right; the *coverage* (IPM, cuts, heuristics depth, presolve
depth) and the *evaluation layer* (R16, Mittelmann, robustness dossier) are what make it
incomplete — and evaluation is what SIH grades first.
