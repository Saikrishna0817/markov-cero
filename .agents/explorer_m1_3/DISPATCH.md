# Dispatch: Milestone 1 Explorer 3 (ADMM Adaptive Rho, Always-On Refinement, NumericalDiagnostic)

Working Directory: /home/saikrishna/markov-initial-build/.agents/explorer_m1_3
Role: teamwork_preview_explorer
Workspace Root: /home/saikrishna/markov-initial-build
Authoritative Request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md
Scope Document: /home/saikrishna/markov-initial-build/PROJECT.md

Task:
Analyze `src/qp/admm_solver.cpp`, `include/markov_cero/qp/admm_solver.hpp`, `src/linalg/sparse_basis.cpp`, `include/markov_cero/api/solve.hpp`, and `src/api/api.cpp`.
Develop an exact implementation strategy for:
1. ADMM adaptive penalty parameter $\rho$:
   Follow Boyd et al. (2011) $\rho \in [10^{-6}, 10^6]$:
   - If $\|r_{prim}\|_\infty > 10 \|r_{dual}\|_\infty$, $\rho \leftarrow \min(2\rho, 10^6)$ and refactorize KKT LDLT.
   - If $\|r_{dual}\|_\infty > 10 \|r_{prim}\|_\infty$, $\rho \leftarrow \max(\rho / 2, 10^{-6})$ and refactorize KKT LDLT.
   - Increment `refactorization_count` in `QpSolution`.
2. Always-on iterative refinement in `src/linalg/sparse_basis.cpp`:
   Always execute at least one refinement pass with `long double` extended precision residual calculation, with early exit when $\|r\|_\infty < 10^{-14}$.
3. Structured `NumericalDiagnostic` in `include/markov_cero/api/solve.hpp`:
   Define `NumericalDiagnostic` containing `primal_residual`, `dual_residual`, `condition_estimate`, `failure_site`, and `suggested_recovery`.
   Embed it in `SolveResult` and populate it across all solver engines (eliminating silent failures).

Deliverable:
Write your report to `/home/saikrishna/markov-initial-build/.agents/explorer_m1_3/analysis.md` and `handoff.md`.

## 2026-09-26T19:26:31Z
You are explorer_m1_3 (teamwork_preview_explorer) for markov-cero.
Your working directory is: /home/saikrishna/markov-initial-build/.agents/explorer_m1_3
Workspace root: /home/saikrishna/markov-initial-build
Authoritative user request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md
Project specification: /home/saikrishna/markov-initial-build/PROJECT.md

Task:
Analyze src/qp/admm_solver.cpp, include/markov_cero/qp/admm_solver.hpp, src/linalg/sparse_basis.cpp, include/markov_cero/api/solve.hpp, and src/api/api.cpp.
Develop an exact implementation strategy for:
1. ADMM adaptive penalty parameter rho: Boyd et al. (2011) rho in [10^-6, 10^6]. Update rho based on primal vs dual residuals (mu=10, tau=2), refactorize KKT LDLT, and increment refactorization_count in QpSolution.
2. Always-on iterative refinement in src/linalg/sparse_basis.cpp: always execute at least one refinement pass with long double residual calculation, exiting early at ||r||_inf < 10^-14.
3. Structured NumericalDiagnostic in include/markov_cero/api/solve.hpp: define NumericalDiagnostic (primal/dual residuals, condition estimate, failure site, suggested recovery), embed in SolveResult, and eliminate silent failures.

Deliverable:
Write your report to /home/saikrishna/markov-initial-build/.agents/explorer_m1_3/analysis.md and your handoff to /home/saikrishna/markov-initial-build/.agents/explorer_m1_3/handoff.md.
When finished, send a completion message back to parent using send_message.

