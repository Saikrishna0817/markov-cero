# Release verification

## What `scripts/verify-release.sh` does

Run from the repository root:

```bash
./scripts/verify-release.sh
```

The script performs, in order:

1. **Environment capture** — writes `evidence/environment-local.json` (OS, machine, Python version).
2. **Sovereignty guard** — runs `scripts/check-sovereignty.py` over the tree (R10: no third-party
   optimization-library linkage or imports).
3. **Dual-compiler build + test** — configures and builds the full CMake project twice
   (GCC and Clang, Release), then runs the CTest suite for each, under a per-run timeout when
   `timeout` is available.

All step output is appended to `evidence/local-verification-report.txt`.

## What a passing report establishes — and what it does not

A passing report establishes only that, **in the local environment**: the sovereignty guard
finds no external-solver contamination, the tree compiles warning-clean under two independent
compilers, and the test suite passes.

It does **not** establish solver correctness on unseen models, numerical robustness on hostile
instances, or any performance claim. For those:

- **Correctness on benchmarks**: `ctest` includes the Netlib/MIPLIB regression targets; the
  cross-solver comparison vs HiGHS (R16) is a separate artifact produced by
  `scripts/run_compare.py` into `evidence/benchmarks/compare/`.
- **Performance claims**: every performance number quoted in docs is bound to the machine
  manifest in `evidence/hardware.md` and a reproducible runner script under `scripts/` or
  `benchmarks/runners/`.

## Regenerating evidence after source changes

The script writes into `evidence/`, which is tracked. When a source change alters any
performance-relevant behavior, re-run the affected runner (not just this script) and commit the
regenerated artifact together with the change.
