---
type: codebase-tech-debt
tags: [codebase, technical-debt, duplication]
severity: low
status: resolved
verified_on: 2026-09-25
resolved_on: 2026-09-25
evidence:
  - "benchmarks/runners/profile_gpu.py"
  - "benchmarks/runners/run_gpu.py"
  - "scripts/profile_gpu.py"
  - "scripts/run_gpu.py"
---

# Duplicate Benchmark Runners

> `benchmarks/runners/*.py` are byte-identical copies of `scripts/*.py`.

## Observed Facts
- `md5sum scripts/profile_gpu.py benchmarks/runners/profile_gpu.py` → both `4ab4111108e04db252371bbcbe21f97e` (size 13535).
- `md5sum scripts/run_gpu.py benchmarks/runners/run_gpu.py` → both `db5d91d1c49d7e0538218fc5a28d1b33` (size 11335).
- Both copies are tracked (`git ls-files benchmarks/` → both files; `git ls-files scripts/` → both files).
- All executable wiring uses the `scripts/` copies: `CMakeLists.txt:215-224`, `.github/workflows/ci.yml:69-79`. No CMake/CI reference to `benchmarks/runners/` (rg over `*.txt`/`*.yml` found none).
- `benchmarks/` contains no other files — no benchmark sources, configs, or data.

## Impact (Inference)
- Any future edit to a runner applied to only one copy silently diverges benchmark methodology between the CTest path and the `benchmarks/` path.
- The `benchmarks/` directory advertises a suite that does not exist beyond the duplicated scripts.

## Resolution (2026-09-25)

Removed both duplicates and the now-empty `benchmarks/` tree per
[[12-keep-remove-rebuild]] §8.2; `scripts/profile_gpu.py` and `scripts/run_gpu.py` are the
sole copies (md5s above verified identical before deletion). No CMake/CI reference existed.

## Related
- [[stale-build-artifacts]] · [[benchmark-suites]] · [[missing-hardware-metadata-in-evidence]]
