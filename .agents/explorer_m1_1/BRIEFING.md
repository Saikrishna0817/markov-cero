# BRIEFING — 2026-09-26T19:37:30Z

## Mission
Analyze IPM dense LU bottleneck and engineer a complete, sovereign SparseLU normal equations factorization strategy for Netlib m >= 200 scaling.

## 🔒 My Identity
- Archetype: explorer
- Roles: explorer
- Working directory: /home/saikrishna/markov-initial-build/.agents/explorer_m1_1
- Original parent: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Milestone: Milestone 1 — Numerical Accuracy Hardening (W5)

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Analyze src/lp/interior/ipm.cpp, include/markov_cero/lp/interior/ipm.hpp, and src/linalg/sparse_basis.cpp
- Develop exact strategy for:
  1. Replacing dense m x m LU in ipm.cpp (lines 320-358) with a sparse normal equations factorizer using SparseLU (src/linalg/sparse_basis.cpp).
  2. Constructing M = A D A^T directly as a SparseCsc matrix where D = diag(x_j / s_j), clamped to [10^-12, 10^12].
  3. Reusing symbolic analysis across IPM iterations when sparsity pattern is fixed.
  4. Enabling IPM to solve Netlib instances with m >= 200 (e.g. sc205, share1b) without memory explosion or singular factorization aborts.
- Clean-room sovereignty preserved (zero third-party solver libraries)

## Current Parent
- Conversation ID: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Updated: 2026-09-26T19:37:30Z

## Investigation State
- **Explored paths**:
  - `src/lp/interior/ipm.cpp`, `include/markov_cero/lp/interior/ipm.hpp`
  - `src/linalg/sparse_basis.cpp`, `include/markov_cero/linalg/sparse_basis.hpp`
  - `src/linalg/dense_lu.cpp`, `include/markov_cero/linalg/dense_lu.hpp`
  - `src/transform/sparse_canonicalize.cpp`, `include/markov_cero/transform/sparse_canonical_model.hpp`
  - `src/api/api.cpp`
  - Netlib benchmark dataset in `data/netlib/` (`sc205`, `share1b`, `adlittle`, `recipe`, `afiro`, `sc50a`, `sc50b`, `sc105`, `share2b`)
- **Key findings**:
  - Root cause 1: `d_row_major` in `ipm.cpp:320` is dense $O(m^2 n)$ assembly and $O(m^2)$ memory, scaling cubically (3.2s on `sc205`, 20GB on $m=50,000$).
  - Root cause 2: `DenseLu::factorize` scales singularity threshold by `maximum_original_entry`, erroneously declaring non-singular matrices singular when $d_j \approx 10^{12}$. The perturbation fallback adds up to $10,000 \cdot I$, destroying the Newton direction and causing "zero step size" aborts (`adlittle`, `recipe`).
  - Root cause 3: All-ones initialization ($x_0 = 1, s_0 = 1$) causes large initial dual residual $r_d = s - c \approx -3000$, forcing initial step size $\alpha \approx 10^{-4}$ and blowing up Mehrotra correction terms (`share1b`).
  - Root cause 4: Roundoff errors in solving $(A D A^T) \Delta y = \text{rhs}$ are magnified by $D \le 10^{12}$ during $\Delta x$ backsubstitution, causing catastrophic loss of primal feasibility without extended-precision iterative refinement.
  - Prototyped solution: Direct sparse $A D A^T$ accumulation + `SparseLu` with absolute pivot tolerance + scale-aware initialization ($x_0 = \max(1, \|b\|_\infty), s_0 = \max(1, \|c\|_\infty)$) + extended precision iterative refinement converged `adlittle` in 15 iterations and `share1b` in 17 iterations.
- **Unexplored areas**: None within the scope of Milestone 1 Explorer 1.

## Key Decisions Made
- Formulate complete architectural strategy for direct sparse CSC $A D A^T$ formation.
- Decouple `SparseLuSymbolicAnalysis` to cache fill-reducing AMD column ordering across all IPM iterations.
- Introduce always-on iterative refinement on normal equation solves using `long double` precision residuals.
- Update `ipm::solve` to accept `SparseCanonicalModel` directly, bypassing the 4,096 row limit in `to_dense()`.

## Artifact Index
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_1/analysis.md` — Complete architectural report and implementation strategy
- `/home/saikrishna/markov-initial-build/.agents/explorer_m1_1/handoff.md` — 5-component handoff report for implementer
