# BRIEFING — 2026-09-27T01:10:00Z

## Mission
Implement Milestone 1 (Numerical Accuracy Hardening) for markov-cero: SparseLU IPM normal equations, PDLP stagnation detection & dual simplex crossover, Boyd et al. (2011) ADMM adaptive rho with refactorization counter, always-on iterative refinement with long double residuals, and structured NumericalDiagnostic across all solver engines.

## 🔒 My Identity
- Archetype: teamwork_preview_worker
- Roles: implementer, qa, specialist
- Working directory: /home/saikrishna/markov-initial-build/.agents/worker_m1_1
- Original parent: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Milestone: Milestone 1 — Numerical Accuracy Hardening (W5)

## 🔒 Key Constraints
- Exclusive write ownership:
  - include/markov_cero/linalg/sparse_basis.hpp, src/linalg/sparse_basis.cpp
  - include/markov_cero/lp/interior/ipm.hpp, src/lp/interior/ipm.cpp
  - include/markov_cero/lp/first_order/pdlp.hpp, src/lp/first_order/pdlp.cpp
  - include/markov_cero/qp/admm_solver.hpp, src/qp/admm_solver.cpp
  - include/markov_cero/api/solve.hpp, src/api/api.cpp, apps/json_output.hpp
  - tests/ and CMakeLists.txt for Milestone 1 verification targets
- Integrity mandate: DO NOT CHEAT. All implementations must be genuine. No hardcoding or dummy implementations.
- Clean-room C++20 sovereignty: Zero external solver dependencies.
- 100% pass rate on all CTest targets with zero regressions.

## Current Parent
- Conversation ID: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Updated: 2026-09-27T01:10:00Z

## Task Summary
- **What to build**:
  1. SparseLU normal equations factorizer in `src/lp/interior/ipm.cpp` and `src/linalg/sparse_basis.cpp`, scaling IPM to m >= 200 without memory explosion or singular aborts.
  2. Windowed stagnation detection (window=1000, threshold=0.999) and dual simplex crossover in `src/lp/first_order/pdlp.cpp`, resolving stalling Netlib instances (kb2, lotfi, beaconfd) to certified KKT <= 10^-7.
  3. Boyd et al. (2011) adaptive penalty rho in [10^-6, 10^6] and refactorization counter in `src/qp/admm_solver.cpp`.
  4. Always-on iterative refinement in `src/linalg/sparse_basis.cpp` with extended precision (`long double`) and early exit at 10^-14.
  5. Structured `NumericalDiagnostic` in `SolveResult` across all solver engines in `src/api/api.cpp` and JSON output in `apps/json_output.hpp`.
  6. Milestone 1 tests in `tests/` and CTest verification.
- **Success criteria**: 100% CTest pass rate, Netlib instances (sc205, share1b, kb2, lotfi, beaconfd) pass certified.
- **Interface contracts**: PROJECT.md Interface Contracts 1-4.
- **Code layout**: PROJECT.md § Code Layout.

## Key Decisions Made
- Follow explorer reports' detailed designs for SparseLU symbolic analysis caching, complementary slackness basis extraction, Boyd adaptive rho update, long double refinement, and universal NumericalDiagnostic.

## Artifact Index
- `.agents/worker_m1_1/DISPATCH.md` — Dispatch assignment
- `.agents/worker_m1_1/progress.md` — Liveness and step tracking
- `.agents/worker_m1_1/handoff.md` — 5-component handoff report

## Change Tracker
- **Files modified**: None yet
- **Build status**: Untested
- **Pending issues**: None

## Quality Status
- **Build/test result**: Pending initial test run
- **Lint status**: Pending
- **Tests added/modified**: Pending

## Loaded Skills
- None required for M1
