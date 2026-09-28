---
type: codebase-test
tags: [codebase, tests, benchmarks]
status: verified
verified_on: 2026-09-25
evidence:
  - "CMakeLists.txt:213-224"
  - "scripts/run_netlib.py:193,229"
  - "scripts/run_gpu.py:21,36"
---

# Benchmark Suite Wiring

> Netlib / MIPLIB / GPU benchmarks are CTest targets that shell out to Python runners.

## Observed Facts
- `netlib_benchmarks` (`CMakeLists.txt:213`) runs `scripts/run_netlib.py --instances afiro adlittle sc50a sc50b sc105 share2b recipe` (7 instances) against `$<TARGET_FILE:markov-cero-solve>`.
- `miplib_benchmarks` (`CMakeLists.txt:214`) runs `scripts/run_miplib.py --instances stein9 stein15 flugpl` (3 instances).
- `gpu_benchmarks` (`CMakeLists.txt:215-219`) runs `scripts/run_gpu.py --instances afiro blend sc50a sc50b` writing `${CMAKE_BINARY_DIR}/reports/gpu_benchmark.csv`; `blend` resolves to Netlib BLEND 75×84 (`scripts/run_gpu.py:21`).
- `gpu_profiling` (`CMakeLists.txt:220-224`) runs `scripts/profile_gpu.py --model examples/blend.mps` → `profile_summary.json` + `profile_analysis.md`.
- Instance data: `data/netlib/` 17 MPS files, `data/miplib/` 3 files, `data/scale_study/` 7 synthetic files (gitignored, `.gitignore:51`), `data/qp/` empty. Runners download to `data/netlib` when missing (`scripts/run_netlib.py:193`).
- Outputs: runners default to `evidence/netlib_results.csv` / `evidence/netlib_extended.csv` (`scripts/run_netlib.py:229,233`); `evidence/benchmarks/` holds `crossover_study.csv`, `phase4.json`, GPU profile artifacts.
- CI invokes the same two runners directly after `ctest` (`.github/workflows/ci.yml:69-79`).
- Each instance is run once: no `repeat`/`median`/`mean` logic exists in `run_netlib.py`, `run_miplib.py`, `run_gpu.py`, `benchmark_phase4_analytics.py` (rg, zero matches).

## Impact (Inference)
- Single-shot timings mean reported `runtime_ms` values (`evidence/netlib_results.csv`) carry unquantified noise; no variance column exists to qualify them.
- `blend` in `gpu_benchmarks` couples a benchmark target to the instance that fails simplex (see [[blend-numerical-failure]]).

## Related
- [[overview]] · [[testing-gaps]] · [[Netlib LP Collection]] · [[negative-parallel-scaling]]
