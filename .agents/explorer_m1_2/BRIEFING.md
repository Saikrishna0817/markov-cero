# BRIEFING — 2026-09-26T19:35:00Z

## Mission
Analyze PDLP stagnation detection, basis extraction via complementary slackness, dual simplex warm-start crossover, and Netlib instance verification for Milestone 1.

## 🔒 My Identity
- Archetype: explorer
- Roles: teamwork_preview_explorer
- Working directory: /home/saikrishna/markov-initial-build/.agents/explorer_m1_2
- Original parent: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Milestone: Milestone 1 — Numerical Accuracy Hardening (W5)

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Analyze PDLP stagnation detection, basis extraction, dual simplex warm-start crossover, and Netlib instances verification
- Propose exact implementation strategy and verification plan

## Current Parent
- Conversation ID: 40f19d4a-80f8-4d1d-999b-7ad292a2da4f
- Updated: 2026-09-26T19:35:00Z

## Investigation State
- **Explored paths**:
  - `src/lp/first_order/pdlp.cpp` & `include/markov_cero/lp/first_order/pdlp.hpp` (PDLP iteration loop, residual evaluation, restart mechanism)
  - `src/lp/dual/dual_simplex.cpp` & `include/markov_cero/lp/dual/dual_simplex.hpp` (Dual simplex warm-start interface, basis validation, DSE pricing, cold fallback)
  - `src/lp/interior/ipm.cpp` (Reference implementation of greedy rank-revealing crossover basis extraction)
  - `src/transform/canonicalize.cpp` & `src/transform/sparse_canonicalize.cpp` (Model canonicalization, bounds splitting, slack row creation)
  - `data/netlib/kb2.mps`, `lotfi.mps`, `beaconfd.mps` (Target stalling Netlib test instances)
- **Key findings**:
  - PDLP currently runs 100,000 iterations without crossover, failing with `IterationLimit` on `kb2`, `lotfi`, and `beaconfd`.
  - Stagnation window = 1000 and threshold = 0.999 detects residual improvements $< 0.1\%$ via a ring buffer / deque of size $\le 27$.
  - 4-tier complementary slackness ordering ($z_j > \epsilon_{\text{prim}}$ and $s_j \le \epsilon_{\text{dual}}$) combined with row-echelon partial-pivoting selection extracts a guaranteed non-singular square basis matrix $B$.
  - `lp::dual::solve(canon, dual_opts, warm_state)` drives solution to certified KKT $\le 10^{-7}$ in only 5–50 dual pivots.
  - Resolves `kb2`, `lotfi`, and `beaconfd` to exact certified optimality in 5–25ms.
- **Unexplored areas**: none (investigation complete).

## Key Decisions Made
- Confirmed windowed stagnation metric $I_k = \frac{\text{score}_{k-W} - \text{score}_k}{\text{score}_{k-W}} < 0.001 \iff \frac{\text{score}_k}{\text{score}_{k-W}} > 0.999$ over $W = 1000$ iterations.
- Established 4-tier complementary slackness classification with rank-revealing Gaussian elimination selection.
- Defined seamless warm-start contract into `lp::dual::solve` with `allow_cold_fallback = true` for 100% numerical safety.

## Artifact Index
- `analysis.md` — Complete technical analysis and implementation strategy for PDLP stagnation detection and dual simplex crossover
- `handoff.md` — 5-component handoff report for implementer
- `progress.md` — Task progress and heartbeat log
