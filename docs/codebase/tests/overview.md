---
type: codebase-test
tags: [codebase, tests, ctest]
status: verified
verified_on: 2026-09-25
evidence:
  - "CMakeLists.txt:166-221"
  - "CMakeLists.txt:104-164"
  - "evidence/local-verification-report.txt:236"
---

# Test Suite Overview

> 43 `add_test` targets live in the root `CMakeLists.txt`; there is no `tests/CMakeLists.txt`.

## Observed Facts
- `add_test` appears only in `CMakeLists.txt:166-221` (43 occurrences; `grep -c add_test CMakeLists.txt` = 43). `tests/CMakeLists.txt` and `gpu/tests/CMakeLists.txt` do not exist (`find . -name CMakeLists.txt` → only root file).
- Test executables: 31 `add_executable` test targets at `CMakeLists.txt:104-164` (23 from `tests/*.cpp` + `tests/fuzz/mps_fuzz_smoke.cpp`, 8 from `gpu/tests/*.cpp`), plus 2 fuzzers gated by `MARKOV_CERO_BUILD_FUZZER` (`CMakeLists.txt:246-256`, Clang-only), plus 3 apps (`CMakeLists.txt:97-101`). Total `add_executable` = 36.
- Suite map (43 tests): 31 unit/component (`build_info`…`qp`, `CMakeLists.txt:166-196`), 1 JSON meta check (`json_records`, `:197`), 1 sovereignty guard (`sovereignty_guard`, `:198-202`), 6 CLI integration (`cli_*`, `:203-211`), 4 benchmark (`netlib_benchmarks` `:213`, `miplib_benchmarks` `:214`, `gpu_benchmarks` `:215-219`, `gpu_profiling` `:220-224`).
- GPU tests (8: `gpu_buffer`, `equivalence`, `gpu_reduction`, 5 × `gpu_pdhg_*`) are registered unconditionally while `MARKOV_CERO_ENABLE_CUDA` defaults OFF (`CMakeLists.txt:7`).
- Framework is hand-rolled (`tests/support/tiny_exact.hpp`, `int main()` + `throw std::runtime_error`); gtest/gmock are explicitly forbidden (`scripts/check-sovereignty.py:23`).
- Last recorded run: `evidence/local-verification-report.txt:236` "100% tests passed out of 41" (UTC 2026-09-21T04:27). Its list lacks `qp` and `cli_qp_portfolio`, so 2 tests were added after that evidence was captured.

## Impact (Inference)
- "43 tests, 100% pass" (`CHANGELOG.md:17`) has no in-repo run artifact for the 43-target state; the only report shows 41.
- Benchmark targets are part of default `ctest`, so CI timing includes I/O-heavy runners (`.github/workflows/ci.yml:66-67`).

## Related
- [[benchmark-suites]] · [[testing-gaps]]
