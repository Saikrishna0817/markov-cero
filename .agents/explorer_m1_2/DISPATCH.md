# Dispatch: Milestone 1 Explorer 2 (PDLP Stagnation & Crossover)

Working Directory: /home/saikrishna/markov-initial-build/.agents/explorer_m1_2
Role: teamwork_preview_explorer
Workspace Root: /home/saikrishna/markov-initial-build
Authoritative Request: /home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md
Scope Document: /home/saikrishna/markov-initial-build/PROJECT.md

Task:
Analyze `src/lp/first_order/pdlp.cpp`, `include/markov_cero/lp/first_order/pdlp.hpp`, and `src/lp/dual/dual_simplex.cpp`.
Develop an exact implementation strategy for:
1. Windowed stagnation detection in PDLP: window = 1000 iterations, threshold = 0.999.
   Track relative residual improvement $\frac{\text{res}_{k} - \text{res}_{k - \text{window}}}{\text{res}_{k - \text{window}}}$. If improvement $< 0.1\%$ over the window, trigger crossover.
2. Basis extraction from PDHG iterates using complementary slackness:
   Identify basic vs non-basic variables using $x_j > \epsilon_{primal}$ and reduced costs / slack values.
3. Warm-starting the dual simplex engine (`lp::dual::solve`) from the extracted basis to drive solution to certified KKT $\le 10^{-7}$.
4. Verifying that previously stalling Netlib test instances (`kb2`, `lotfi`, `beaconfd`) resolve to certified optimality.

Deliverable:
Write your report to `/home/saikrishna/markov-initial-build/.agents/explorer_m1_2/analysis.md` and `handoff.md`.
