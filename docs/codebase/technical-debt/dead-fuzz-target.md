---
type: codebase-tech-debt
tags: [codebase, technical-debt, fuzzing]
severity: low
status: resolved
verified_on: 2026-09-25
resolved_on: 2026-09-25
evidence:
  - "tests/fuzz/mps_coverage_fuzz.cpp"
  - "CMakeLists.txt:246-256"
  - "CMakeLists.txt:110,169"
---

# Dead Fuzz Target

> `tests/fuzz/mps_coverage_fuzz.cpp` is never compiled by any CMake target.

## Observed Facts
- `tests/fuzz/` holds 4 sources: `mps_fuzz.cpp`, `mps_fuzz_smoke.cpp`, `mps_coverage_fuzz.cpp`, `sparse_basis_fuzz.cpp`.
- CMake references exactly 3: `mps_fuzz_smoke` (`CMakeLists.txt:110`, registered as a test at `:169`), and under `if(MARKOV_CERO_BUILD_FUZZER)` the targets `mps_fuzz` (`:250`) and `sparse_basis_fuzz` (`:254`).
- `rg "mps_coverage_fuzz"` across `*.txt`, `*.cmake`, `*.yml`, `*.py`, `*.md` matches only `docs/audit/00-ground-truth.md:197` — no build or CI reference.
- Fuzzer targets are Clang-only (`CMakeLists.txt:247-249`) and not enabled by `.github/workflows/ci.yml` (no `MARKOV_CERO_BUILD_FUZZER` flag anywhere in that file).

## Impact (Inference)
- The coverage-guided MPS fuzzer code rots silently: compile errors or API drift would go unnoticed.
- Effective fuzzing in CI is limited to the non-fuzzing `mps_fuzz_smoke` regression input.

## Resolution (2026-09-25)

Decision: **remove** (not re-wire) — `tests/fuzz/mps_coverage_fuzz.cpp` and
`scripts/generate-fuzz-corpus.py` deleted per [[12-keep-remove-rebuild]] §8.2. The three
wired fuzz targets (`mps_fuzz_smoke`, and `mps_fuzz`/`sparse_basis_fuzz` under
`MARKOV_CERO_BUILD_FUZZER`) are untouched. Re-investigating coverage fuzzing stays a P2
option (INVESTIGATE row).

## Related
- [[stale-build-artifacts]] · [[mps-parser-limitations]] · [[testing-gaps]]
