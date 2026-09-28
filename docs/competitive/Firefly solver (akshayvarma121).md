---
type: competitive-note
tags: [competitive, t1, sih26119]
status: verified
source: github.com/akshayvarma121/Firefly_solver
cloned: 2026-09-25
threat: medium
---

# Firefly solver (akshayvarma121)

> **Rank 9.** Real CUDA LP+MILP code behind a slick web UI — but its "HiGHS Reference" is
> **two hardcoded numbers**, there is no CI, and 98% of tracked files are committed build
> output. Threat = demo flash, not evidence.

## What's real [Observed]

- Core (non-build): ~24 .cpp + 1 .cu + 7 .h: PDHG/PDLP first-order LP with adaptive steps,
  restarts, Ruiz equilibration — CPU (`pdlp_cpu.cpp`) and GPU (`pdlp_cuda.cu`, cuSPARSE/cuBLAS,
  sm_89, no-GPU fallback, cpu/gpu parity test); revised **primal** simplex (`simplex.cpp`
  15.7 KB, Bland anti-cycling); branch-and-bound (`branch_and_bound.cpp` 9.1 KB, node
  callbacks); presolve 15.1 KB; MPS-only parser; pybind11 bindings 21.4 KB; FastAPI `api/main.py`;
  React telemetry UI; 15 test .cpp + 32 .mps + 11 CTest targets.
- R10 clean: FetchContent **Eigen 3.4.0** + CUDA runtime only; `web/src/pages/Architecture.tsx:168`
  "No HiGHS, GLPK, or CBC in core/". MIT (`LICENSE`, "The Fireflies").

## The evidence problem (R16 = label only) [Observed]

- `api/main.py:35-38`:
  ```
  _BENCHMARK_REFERENCES = {
      "test_problem1.mps": -10.0,
      "test_problem2.mps": -12.0,
  }
  ```
- `web/src/pages/Benchmarks.tsx:84,235,258` labels these **"HiGHS Reference"** /
  "Reference objective values are from HiGHS" — no HiGHS binary or library anywhere;
  HiGHS appears only as a *host for Netlib instance files* (`scripts/download_instances.py:27`).
- `core/audit.sh:502-525`: benchmark suite looks for `data/netlib|miplib|qplib` which don't
  exist → `:517-519` prints `[SKIP] … benchmark data not found` — **the audit harness always
  skips every benchmark suite** (real data sits in `core/tests/netlib_miplib/`, name mismatch).
- No `.github/` → no CI; `core/run_benchmark.ps1` hardcodes `E:\firefly\...exe`.

## Hygiene

- **8564 of 8718 tracked files (98%) are under `core/build_cpu|build_cuda|build_asan`** —
  ~102 MB including vendored Eigen `_deps/eigen-src`; 20 `__pycache__/*.pyc` despite
  `.gitignore`; `.gitignore` also ignores `AGENTS.md`.

## Claims vs code

README QP claim: **[Not found]** in core (no QP engine located); LP-format reader claimed,
MPS-only implemented; no IPM, no cuts, no parallel.

**Respect:** working CUDA parity tests + a demo-able API/web surface (evaluator eye-candy).
**Beat:** our verifiers + committed evidence vs their hardcoded references — if anyone
side-by-sides their "Benchmarks" page with ours, only ours re-runs.
Related: [[19-competitive-landscape]] §19.6, §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]