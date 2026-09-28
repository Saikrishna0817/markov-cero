# Dispatch: Milestone 1 Implementation Worker Replacement (worker_m1_2)

Working Directory: /home/saikrishna/markov-initial-build/.agents/worker_m1_2
Role: teamwork_preview_worker
Workspace Root: /home/saikrishna/markov-initial-build
Authoritative Request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md
Scope Document: /home/saikrishna/markov-initial-build/PROJECT.md
Test Infrastructure: /home/saikrishna/markov-initial-build/TEST_INFRA.md

Explorer Reports:
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_1/analysis.md`
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_1/handoff.md`
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_2/analysis.md`
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_2/handoff.md`
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_3/analysis.md`
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_3/handoff.md`

MANDATORY INTEGRITY WARNING:
> DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

File Ownership:
You have exclusive write ownership of:
- `include/markov_cero/linalg/sparse_basis.hpp`, `src/linalg/sparse_basis.cpp`
- `include/markov_cero/lp/interior/ipm.hpp`, `src/lp/interior/ipm.cpp`
- `include/markov_cero/lp/first_order/pdlp.hpp`, `src/lp/first_order/pdlp.cpp`
- `include/markov_cero/qp/admm_solver.hpp`, `src/qp/admm_solver.cpp`
- `include/markov_cero/api/solve.hpp`, `src/api/api.cpp`, `apps/json_output.hpp`
- `tests/` and `CMakeLists.txt` for Milestone 1 verification targets

Implementation Plan:
1. **SparseLU Normal Equations for IPM (`src/lp/interior/ipm.cpp`, `src/linalg/sparse_basis.cpp`)**:
   - Construct $M = A D A^T$ directly as a `SparseCsc` matrix in $O(\text{nnz}(A))$ time using precomputed $A^T$ and $d_j = \text{clamp}(x_j / s_j, 10^{-12}, 10^{12})$.
   - Add `SparseLuSymbolicAnalysis` to cache fill-reducing column permutation (AMD) across iterations.
   - Replace dense LU in `ipm.cpp` with `SparseLu::factorize(M)`.
   - Apply extended-precision iterative refinement on $(A D A^T) \Delta y = \text{rhs}$ with early exit at $\|r\|_\infty < 10^{-14}$.
   - Add scale-aware initialization ($x_0 = \max(1, \|b\|_\infty), s_0 = \max(1, \|c\|_\infty)$), gentle regularizer $\delta = 10^{-12}$, and decoupled primal/dual step sizes ($\alpha_p, \alpha_d$).
   - Verify that Netlib instances with $m \ge 200$ (e.g. `sc205`, `share1b`, `adlittle`, `recipe`) solve cleanly without memory explosion or zero-step aborts.

2. **PDLP Stagnation Detection & Dual Simplex Crossover (`src/lp/first_order/pdlp.cpp`)**:
   - Implement sliding window stagnation detection: window $W = 1000$ iterations, threshold ratio $0.999$.
   - When stagnation is detected (improvement $< 0.1\%$ over window), extract candidate basis via complementary slackness ($x_j > \epsilon_{primal}, s_j \le \epsilon_{dual}$).
   - Warm-start dual simplex solver (`lp::dual::solve`) from the extracted basis to drive solution to certified KKT $\le 10^{-7}$.
   - Verify that previously stalling Netlib test instances (`kb2`, `lotfi`, `beaconfd`) resolve to certified optimality.

3. **ADMM Adaptive Penalty $\rho$ & Refactorization Tracking (`src/qp/admm_solver.cpp`, `include/markov_cero/qp/admm_solver.hpp`)**:
   - Add `std::size_t refactorization_count{0};` to `QpSolution`.
   - Update $\rho \in [10^{-6}, 10^6]$ according to Boyd et al. (2011) at adaptive interval (every 25 iterations):
     - If $\|r_{prim}\|_\infty > 10 \|r_{dual}\|_\infty$: $\rho \leftarrow \min(2\rho, 10^6)$.
     - If $\|r_{dual}\|_\infty > 10 \|r_{prim}\|_\infty$: $\rho \leftarrow \max(\rho/2, 10^{-6})$.
   - On $\rho$ change, update numeric KKT factorization, increment `refactorization_count`.

4. **Always-On Iterative Refinement (`src/linalg/sparse_basis.cpp`)**:
   - Unconditionally invoke `refine()` whenever `options_.maximum_refinement_steps > 0`.
   - In `refine()`, use `long double` accumulator and place early exit `if (worst < 1e-14) break;` before incrementing `refinement_attempts` so exact integer/Moler bases exit at step 0 with 0 attempts.

5. **Structured NumericalDiagnostic Across All Solver Engines (`include/markov_cero/api/solve.hpp`, `src/api/api.cpp`)**:
   - Define `NumericalDiagnostic` with `primal_residual`, `dual_residual`, `condition_estimate`, `failure_site`, `suggested_recovery`.
   - Embed in `SolveResult` and populate in every engine.
   - Enforce non-empty `failure_site` and `suggested_recovery` for any non-optimal solve.
   - Serialize into `apps/json_output.hpp`.

6. **Build & Verify**:
   - Compile code and run 100% CTest targets. Ensure 0 regressions.
   - Test Netlib `sc205`, `share1b`, `adlittle`, `recipe`, `kb2`, `lotfi`, `beaconfd`.
   - Document build commands, test results, and verification output in `handoff.md`.

## 2026-09-27T00:30:20Z
**From**: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f (parent)
**Context**: Milestone 1 Implementation Status Check
**Content**: Checking on current progress of Milestone 1 implementation tasks (SparseLU IPM, PDLP crossover, ADMM adaptive rho, Refinement, Diagnostics).
**Action**: Please update progress.md with your latest step and status.
