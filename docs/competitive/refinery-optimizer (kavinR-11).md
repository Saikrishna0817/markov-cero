---
type: competitive-note
tags: [competitive, t1, sih26119]
status: verified
source: github.com/kavinR-11/refinery-optimizer
cloned: 2026-09-25
threat: medium-high
---

# refinery-optimizer (kavinR-11)

> **Rank 5.** The only competitor publishing an HiGHS *win* — and the only serious one with
> **zero CI** and **no LICENSE file** despite README claiming MIT.

## Engines [Observed]

- **IPM (R4)**: real Mehrotra predictor-corrector — `core/src/ipm/ipm_solver.cpp:178`
  ("Main Mehrotra Predictor-Corrector Loop", 594 L) + **crossover**
  `core/src/ipm/crossover.cpp` (142 L) tested by `tests/test_ipm_crossover.py`.
- **Parallel (R7)**: custom `std::thread` pool `core/include/sih/bnb/thread_pool.hpp:16,99-102`
  engaged at `core/src/bnb/branch_and_bound.cpp:393-394`; OpenMP sparse ops
  (`core/src/model/sparse_matrix.cpp:94,119`).
- **GPU (R8)**: `core/src/gpu/cuda_pdhg.cu` (208 L) **FP32 only** — kernels take `const float*`
  (`:17-19,55-61`); README:36 "FP32 PDHG + FP64 Hybrid Simplex"; claims 36× vs CPU simplex
  (unverified against their own committed data).
- Simplex: dual + primal with steepest-edge/Devex/Harris/Bland, sparse LU Markowitz +
  Forrest–Tomlin; B&B with root GMI cuts, diving/rounding, pseudocost, warm starts;
  presolve/postsolve reversible stack; refinery plant model + shadow-price explainability.

## Evidence (R16 — committed but not reproducible) [Observed]

- README §5.1 (`README.md:140-145`): Netlib LP 9 inst SGM 9.64 ms vs 1.78 ms (22.2% win);
  **MIPLIB 5 inst 984 ms vs 2367 ms (80% win)**; QP 10 inst 7.86 vs 1.57 ms (50%).
- Those exact numbers exist in `bench/benchmark_results.json` (generated 2026-09-24,
  per-instance `obj_diff`), written by `bench/run_full_benchmark_suite.py:315,353`.
- **Caveat:** `data/netlib`, `data/miplib`, `data/maros_meszaros` are **empty dirs** — the
  suite does not run from a fresh clone without `tools/download_*.py`; no CI to re-run it.
- Oracle: `tests/oracle/highs_oracle.py:8` `import highspy`, quarantined via
  `tools/check_no_external_solvers.py` `EXEMPT_DIRS :17-27`.

## Weak spots

- **No CI whatsoever** [Observed: no `.github/`, no gitlab/travis/circle/azure/Jenkinsfile].
- **No LICENSE file** — README:245-247 claims MIT, badge links dangle.
- Benchmark inputs absent → their 80% win cannot be independently re-run today.
- `check_no_external_solvers.py` is line-based regex (`:30-49, :95-107`), weaker than an AST
  scan or an ldd gate (see [[SANKHYA (thegoodengineers)]] `ci.yml:814`).

## R10 check

No solver in `CMakeLists.txt`/`core/`; highspy only in exempted bench/test paths. Clean in
substance, informal in enforcement.

**Respect:** IPM+parallel+GPU in one repo, domain-aligned (refinery) like our R11 story.
**Beat:** reproducibility — empty data dirs + no CI means an evaluator cannot verify their
headline win; our `run_compare.py` + committed instances would out-trust them instantly.
Related: [[19-competitive-landscape]] §19.5, §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]