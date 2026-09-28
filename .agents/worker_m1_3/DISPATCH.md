# Dispatch: Milestone 1 Implementation Worker (worker_m1_3)

Working Directory: /home/saikrishna/markov-initial-build/.agents/worker_m1_3
Role: teamwork_preview_worker
Workspace Root: /home/saikrishna/markov-initial-build
Authoritative Request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md
Scope Document: /home/saikrishna/markov-initial-build/PROJECT.md
Test Infrastructure: /home/saikrishna/markov-initial-build/TEST_INFRA.md

Explorer Reports with Exact Implementation Blueprints:
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_1/analysis.md` (SparseLU IPM)
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_2/analysis.md` (PDLP Stagnation & Crossover)
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_3/analysis.md` (ADMM Adaptive Rho, Refinement, Diagnostics)

MANDATORY INTEGRITY WARNING:
> DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

File Ownership:
You have exclusive write ownership of:
- `include/markov_cero/linalg/sparse_basis.hpp`, `src/linalg/sparse_basis.cpp`
- `include/markov_cero/lp/interior/ipm.hpp`, `src/lp/interior/ipm.cpp`
- `include/markov_cero/lp/first_order/pdlp.hpp`, `src/lp/first_order/pdlp.cpp`
- `include/markov_cero/qp/admm_solver.hpp`, `src/qp/admm_solver.cpp`
- `include/markov_cero/api/solve.hpp`, `src/api/api.cpp`, `apps/json_output.hpp`
- `tests/` and `CMakeLists.txt` for Milestone 1 targets

Execution Protocol:
Execute the implementation incrementally and update `progress.md` after each step:

Step 1: Always-On Iterative Refinement in `src/linalg/sparse_basis.cpp`:
- Implement unconditional refinement when `maximum_refinement_steps > 0`.
- In `refine()`, use `long double` accumulator and place early exit `if (worst < 1e-14) break;` before incrementing `refinement_attempts`.
- Build and verify `sparse_basis_test`.

Step 2: Boyd et al. (2011) Adaptive Rho in `src/qp/admm_solver.cpp`:
- Add `std::size_t refactorization_count{0};` to `struct QpSolution` in `admm_solver.hpp`.
- In `admm_solver.cpp`, update $\rho \in [10^{-6}, 10^6]$ adaptively at interval (every 25 iter):
  * If $\|r_{prim}\|_\infty > 10 \|r_{dual}\|_\infty$: $\rho \leftarrow \min(2\rho, 10^6)$.
  * If $\|r_{dual}\|_\infty > 10 \|r_{prim}\|_\infty$: $\rho \leftarrow \max(\rho/2, 10^{-6})$.
- On change, call `kkt.update_numeric()`, increment `sol.refactorization_count`.
- Build and verify `admm_solver_test`.

Step 3: Structured `NumericalDiagnostic` in `include/markov_cero/api/solve.hpp`, `src/api/api.cpp`, `apps/json_output.hpp`:
- Define `NumericalDiagnostic` with `primal_residual`, `dual_residual`, `condition_estimate`, `failure_site`, `suggested_recovery`.
- Embed in `SolveResult` and populate in every engine.
- Serialize into JSON output.
- Build and verify solve API tests.

Step 4: PDLP Stagnation Detection & Dual Simplex Crossover in `src/lp/first_order/pdlp.cpp`:
- Sliding window $W=1000$ iterations, ratio $> 0.999$.
- On stagnation, extract basis via complementary slackness ($x_j > \epsilon_{primal}, s_j \le \epsilon_{dual}$) and warm-start `lp::dual::solve`.
- Verify `kb2`, `lotfi`, `beaconfd`.

Step 5: SparseLU Normal Equations in `src/lp/interior/ipm.cpp`:
- Replace dense $m \times m$ LU with sparse $A D A^T$ using `SparseLu`.
- Precompute $A^T$ to construct $M$ in $O(\text{nnz}(A))$.
- Cache symbolic analysis across iterations.
- Scale-aware initialization ($x_0 = \max(1, \|b\|_\infty), s_0 = \max(1, \|c\|_\infty)$).
- Verify Netlib instances (`sc205`, `share1b`, `adlittle`, `recipe`).

Step 6: Verification:
- Run full CTest suite (`ctest --test-dir build --output-on-failure`).
- Run E2E runner (`python3 scripts/run_e2e_tests.py --build-dir build`).
- Verify clean-room sovereignty (`python3 scripts/check-sovereignty.py . --binary build/markov-cero-solve`).
- Write `handoff.md` and report to parent.

## 2026-09-27T00:40:29Z
You are worker_m1_3 (teamwork_preview_worker) for markov-cero.
Your working directory is: /home/saikrishna/markov-initial-build/.agents/worker_m1_3
Workspace root: /home/saikrishna/markov-initial-build
Authoritative user request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md
Project specification: /home/saikrishna/markov-initial-build/PROJECT.md
Test infrastructure: /home/saikrishna/markov-initial-build/TEST_INFRA.md
