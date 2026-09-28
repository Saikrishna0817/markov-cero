---
type: competitive-note
tags: [competitive, t1, sih26119]
status: verified
source: github.com/Satyanshgaur/PIPEPYE
cloned: 2026-09-25
threat: medium
---

# PIPEPYE (Satyanshgaur)

> **Rank 7.** A genuinely quantified R16 the project mostly *loses* (HiGHS wins 18/32) —
> scores honesty, not capability. Medium threat: solid CI, no IPM, thin GPU.

## Engines [Observed]

- Dual steepest-edge/Devex simplex + restarted **PDHG (CUDA sm_86, RTX 3050)** + B&B with
  warm-start dual reoptimization + crossover; presolve 5 passes + postsolve; Ruiz/
  Pock–Chambolle scaling; structure-aware `SolverSelector`.
- GPU kernels limited to DAXPY/SAXPY + SpMV; **no IPM**; no MIP in-tree cut loop found.
- Parallel: OpenMP sparse ops (`src/sparse/cpu_ops.cpp:68,103,128,144`).
- Scale: 84 C/C++ + 59 H, 33.2k LOC; GoogleTest claimed 174 tests.

## Evidence (R16 — genuine, self-undercutting) [Observed]

- `reports/external_solver_benchmark.csv`: **32 data rows**, `winner` column =
  **HiGHS 18, PipePye 14** (LP 19 / MILP 13); per-instance `highs_simplex_iters`,
  `highs_mip_nodes`, `ratio_pipepye_to_highs`.
- `reference_solutions.json`: `"solver":"HiGHS-1.8.1"` with independent verification.
- Produced by commit "add bare-metal external performance baseline against HiGHS 1.15.1";
  `scripts/solve_and_verify_reference.py:6,79` uses `highspy` (bench-only).
- `docs/architecture.md:170-178`: external solvers declared "references, not dependencies".

## CI [Observed]

`.github/workflows/ci.yml`: job `build-and-test-cpu` (Ubuntu 24.04, CMake/Ninja/CTest) + job
`build-and-test-cuda` (Ubuntu 22.04, `Jimver/cuda-toolkit@v0.2.19`, sm_86, GoogleTest) +
`deploy-pages.yml`. Real CPU+CUDA CI — rarer than it sounds in this field.

## Weak spots

- **No LICENSE file** anywhere (`git ls-files | grep -i licen` → none) — MIT claimed? unclaimed?
- No IPM (R4), no in-tree cut loop (R5), GPU kernels primitive.
- Their own CSV says they lose to HiGHS on 56% of instances — fine for honesty, bad for a
  "sovereign replacement" pitch.

## R10 check

No solver in `CMakeLists.txt`/`src/`; `docs/environment.md:43` "Zero External Solver
Dependencies… no linkage to GLPK, COIN-OR, HiGHS". Clean.

**Respect:** per-instance ratio format (same format we should adopt for `evidence/compare/`).
**Beat:** capability breadth (IPM, cuts, QP) — they simply don't have it.
Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]