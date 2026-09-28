# Dispatch: Milestone 1 Explorer 1 (SparseLU IPM Normal Equations)

Working Directory: /home/saikrishna/markov-initial-build/.agents/explorer_m1_1
Role: teamwork_preview_explorer
Workspace Root: /home/saikrishna/markov-initial-build
Authoritative Request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md
Scope Document: /home/saikrishna/markov-initial-build/PROJECT.md

Task:
Analyze `src/lp/interior/ipm.cpp`, `include/markov_cero/lp/interior/ipm.hpp`, and `src/linalg/sparse_basis.cpp`.
Develop an exact implementation strategy for:
1. Replacing dense $m \times m$ LU in `ipm.cpp` (lines 320-358) with a sparse normal equations factorizer using `SparseLU` (`src/linalg/sparse_basis.cpp`).
2. Constructing $M = A D A^T$ directly as a `SparseCsc` matrix where $D = \text{diag}(x_j / s_j)$, clamped to $[10^{-12}, 10^{12}]$.
3. Reusing symbolic analysis across IPM iterations when sparsity pattern is fixed.
4. Enabling IPM to solve Netlib instances with $m \ge 200$ (e.g. `sc205`, `share1b`) without memory explosion or singular factorization aborts.

Deliverable:
Write your report to `/home/saikrishna/markov-initial-build/.agents/explorer_m1_1/analysis.md` and `handoff.md`.
