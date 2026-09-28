# BRIEFING — 2026-09-26T19:37:00Z

## Mission
Investigate ADMM adaptive penalty rho, always-on iterative refinement in sparse_basis, and structured NumericalDiagnostic across markov-cero solver engines.

## 🔒 My Identity
- Archetype: explorer
- Roles: teamwork_preview_explorer, numerical_investigator
- Working directory: /home/saikrishna/markov-initial-build/.agents/explorer_m1_3
- Original parent: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Milestone: Milestone 1 — Numerical Accuracy Hardening (W5)

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Analyze src/qp/admm_solver.cpp, include/markov_cero/qp/admm_solver.hpp, src/linalg/sparse_basis.cpp, include/markov_cero/api/solve.hpp, and src/api/api.cpp
- Write analysis.md and handoff.md in /home/saikrishna/markov-initial-build/.agents/explorer_m1_3/
- Send completion message to parent (40f19d4a-80f8-4d1d-999b-7ad292a2da4f)

## Current Parent
- Conversation ID: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Updated: 2026-09-26T19:26:31Z

## Investigation State
- **Explored paths**:
  - `src/qp/admm_solver.cpp` and `include/markov_cero/qp/admm_solver.hpp`
  - `src/qp/kkt.cpp` and `include/markov_cero/qp/kkt.hpp`
  - `src/linalg/sparse_basis.cpp` and `include/markov_cero/linalg/sparse_basis.hpp`
  - `include/markov_cero/api/solve.hpp` and `src/api/api.cpp`
  - `apps/markov_cero_solve.cpp` and `apps/json_output.hpp`
  - `tests/qp_test.cpp`, `tests/sparse_basis_test.cpp`, `tests/api_test.cpp`, `examples/api_demo.cpp`
- **Key findings**:
  1. ADMM currently uses OSQP heuristic ratio scaling clamped to [1e-3, 1e4]; Boyd et al. (2011) adaptation (mu=10, tau=2, rho in [1e-6, 1e6]) preserves KKT sparsity pattern and enables numeric LDLT updates in O(nnz(L)) time while accurately incrementing refactorization_count.
  2. Iterative refinement in sparse_basis is currently selective; always-on refinement with long double residuals and early exit at ||r||_inf < 1e-14 guarantees zero forward/backward error drift while keeping attempt counts at 0 for exact/clean bases (preserving test suite invariants).
  3. Structured NumericalDiagnostic struct defined in solve.hpp, embedded in SolveResult, mapped across all engines, and guaranteed non-empty in finalize() eliminates silent failures.
  4. CTest baseline verified: 59/59 tests passing (100%).
- **Unexplored areas**: None within the assigned M1 scope.

## Key Decisions Made
- Confirmed unscaled dual variable y remains invariant when rho changes.
- Placed early exit check before refinement_attempts counter to preserve Moler/clean matrix test assertions.
- Designed comprehensive diagnostic mapping covering QP, PDLP, simplex, IPM, MILP, parser, and verification failures.

## Artifact Index
- `.agents/explorer_m1_3/DISPATCH.md` — Task dispatch and prompt history
- `.agents/explorer_m1_3/BRIEFING.md` — Situational awareness
- `.agents/explorer_m1_3/progress.md` — Heartbeat log
- `.agents/explorer_m1_3/analysis.md` — Authoritative technical analysis report
- `.agents/explorer_m1_3/handoff.md` — 5-component handoff report
