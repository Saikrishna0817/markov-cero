# BRIEFING — 2026-09-27T00:15:00Z

## Mission
Implement and verify Milestone 1 (Numerical Accuracy Hardening) for markov-cero: SparseLU normal equations for IPM, PDLP windowed stagnation detection and dual simplex crossover, Boyd et al. (2011) ADMM adaptive rho and refactorization counter, always-on iterative refinement with extended precision, and structured NumericalDiagnostic across all solver engines.

## 🔒 My Identity
- Archetype: teamwork_preview_worker
- Roles: implementer, qa, specialist
- Working directory: /home/saikrishna/markov-initial-build/.agents/worker_m1_2
- Original parent: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Milestone: Milestone 1 — Numerical Accuracy Hardening (W5)

## 🔒 Key Constraints
- Exclusive file ownership:
  - include/markov_cero/linalg/sparse_basis.hpp, src/linalg/sparse_basis.cpp
  - include/markov_cero/lp/interior/ipm.hpp, src/lp/interior/ipm.cpp
  - include/markov_cero/lp/first_order/pdlp.hpp, src/lp/first_order/pdlp.cpp
  - include/markov_cero/qp/admm_solver.hpp, src/qp/admm_solver.cpp
  - include/markov_cero/api/solve.hpp, src/api/api.cpp, apps/json_output.hpp
  - tests/ and CMakeLists.txt for Milestone 1 verification targets
- Integrity Mandate: No hardcoded test results, no dummy/facade implementations, genuine logic only.
- Clean-room sovereignty: C++20 standard library only; zero prohibited external dependencies.
- 100% CTest pass rate with zero regressions.

## Current Parent
- Conversation ID: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Updated: not yet

## Task Summary
- **What to build**:
  1. SparseLU normal equations factorizer in src/lp/interior/ipm.cpp and src/linalg/sparse_basis.cpp.
  2. Windowed stagnation detection and dual simplex crossover in src/lp/first_order/pdlp.cpp.
  3. Boyd et al. (2011) adaptive rho and refactorization counter in src/qp/admm_solver.cpp.
  4. Always-on iterative refinement in src/linalg/sparse_basis.cpp.
  5. Structured NumericalDiagnostic in SolveResult across all engines in src/api/api.cpp, include/markov_cero/api/solve.hpp, apps/json_output.hpp.
  6. Unit and integration tests in tests/, verifying Netlib instances.
- **Success criteria**:
  - Netlib m >= 200 (sc205, share1b, adlittle, recipe) solve cleanly in IPM.
  - kb2, lotfi, beaconfd resolve to certified KKT <= 10^-7 in PDLP.
  - ADMM refactorization counter correctly tracked.
  - Always-on refinement with long double accumulator early exit at 10^-14.
  - Non-optimal solves produce detailed NumericalDiagnostic.
  - 100% test pass rate on all CTest targets.
- **Interface contracts**: PROJECT.md Section: Interface Contracts
- **Code layout**: PROJECT.md

## Change Tracker
- **Files modified**: None yet
- **Build status**: Unchecked (investigating initial build)
- **Pending issues**: None

## Quality Status
- **Build/test result**: Pending initial test run
- **Lint status**: Clean
- **Tests added/modified**: None yet

## Loaded Skills
- None

## Key Decisions Made
- Follow the architectural guidelines from explorer_m1_1, explorer_m1_2, and explorer_m1_3.
- Build and verify baseline tests first.

## Artifact Index
- DISPATCH.md — Assignment and instructions
- BRIEFING.md — Situational awareness and working memory
- progress.md — Liveness heartbeat and progress tracking
- handoff.md — 5-component handoff report upon completion
