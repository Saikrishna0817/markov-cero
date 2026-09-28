---
type: codebase-decision
tags: [codebase, decision, dependencies, governance]
status: verified
verified_on: 2026-09-25
evidence:
  - "docs/architecture.md:10-11"
  - "scripts/check-sovereignty.py:18-47"
  - "CMakeLists.txt:198-202"
  - "third_party/LICENSES/README.md"
---

# No External Solver Dependency

> The build links only the C++20 standard library, threads, and optional CUDA; a CTest target enforces it.

## Observed Facts
- Policy: `docs/architecture.md:10-11` — "Zero code, data, symbols, or bindings from third-party optimization libraries (HiGHS, GLPK, Clp, SCIP, Gurobi, CPLEX, etc.). Only standard C++20 and POSIX threads."
- Allowlists in `scripts/check-sovereignty.py`: `FORBIDDEN_LIBRARIES` (`:18-24`, incl. eigen/boost/gtest/nlohmann), `FORBIDDEN_HEADERS` (`:26-31`), `FORBIDDEN_SYMBOLS` (`:33-36`), `ALLOWED_DYNAMIC_LIBS` (`:38-43`), `ALLOWED_FIND_PACKAGES = {threads, cuda, cudatoolkit}` (`:45-46`).
- Three check layers: `check_cmake` (`:49-84`, rejects disallowed `find_package`, forbidden link targets, and `FetchContent`/`ExternalProject_Add`/`add_subdirectory`), `check_source_tree` (`:87-136`, scans `src/include/apps/tests/gpu` includes plus `third_party/vendor/extern/submodules` for code/binary files), `inspect_binary` (`:139-188`, `readelf -d` NEEDED allowlist).
- Wired into CI as `sovereignty_guard` (`CMakeLists.txt:198-202`) and invoked first in the local verification log (`evidence/local-verification-report.txt:4`).
- `third_party/` contains only `LICENSES/README.md` — no vendored code (`ls -R third_party/`).

## Impact (Inference)
- Satisfies R10/R18 by construction and makes accidental dependency creep a test failure, not a review opinion.
- The same constraint forbids gtest, which explains the hand-rolled test harness ([[overview]]) and precludes reusing existing solver test suites.

## Related
- [[clean-room-provenance]] · [[no-external-baseline]] · [[testing-gaps]]
