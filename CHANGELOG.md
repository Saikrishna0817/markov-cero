# Changelog

## Unreleased

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
- **RW-10 (claims layer)**: added the R1–R20 coverage matrix to `STATUS.md` (12 MET / 8 PARTIAL,
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
- **Docs/claims layer** — `STATUS.md` gains the AP-1–AP-12 closure register and R4 moves to
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
- Added `markov-cero-solve`, refinery qualification models, and `run-qualification-demo.sh`.
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
