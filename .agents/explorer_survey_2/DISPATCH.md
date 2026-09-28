# Dispatch for Explorer Survey 2 (explorer_codebase)

Working Directory: /home/saikrishna/markov-initial-build/.agents/explorer_survey_2
Role: teamwork_preview_explorer
Workspace Root: /home/saikrishna/markov-initial-build
Authoritative Request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md

Task:
Perform a comprehensive survey of the existing codebase:
- `CMakeLists.txt` and all subproject CMake configurations
- `include/` and `src/` directory structures and current header interfaces
- Current implementations in:
  * `src/lp/interior/ipm.cpp` (dense LU vs sparse normal equations requirements)
  * `src/lp/first_order/pdlp.cpp` (current iteration loop, restart, stagnation check)
  * `src/qp/admm_solver.cpp` (ADMM implementation, penalty parameter rho)
  * `src/linalg/sparse_basis.cpp` (SparseLU factorizer and iterative refinement)
  * `src/model/` (existing model representations, MPS parser, SolveResult struct, NumericalDiagnostic)
  * `src/nlp/` and `src/minlp/` (existing files or stubs)
  * `gpu/` (existing CUDA setup, device.cpp, kernels)
  * `python/` (existing setup, pybind11 integration)
  * `tests/` (existing 44 CTest targets, test framework, how tests are invoked and structured)

Deliverable:
Write a detailed report at `/home/saikrishna/markov-initial-build/.agents/explorer_survey_2/codebase_report.md` and `handoff.md`.
Report exact file paths, existing function signatures, data structures, test targets, and gaps relative to ORIGINAL_REQUEST.md.
