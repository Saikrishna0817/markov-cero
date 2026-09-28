---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/milp/milp_solver.cpp:22"
  - "include/markov_cero/milp/milp_solver.hpp:47"
verified_on: 2026-09-25
---

# BranchAndCut

> Single-threaded branch-and-cut driver: root heuristics → root cuts → strong branching → best-bound tree search with in-tree cut separation.

## Responsibility
- Orchestrate LP/QP node relaxations, primal heuristics, cut rounds, branching and bound/gap termination into `milp::Result`.

## Implementation Facts (Observed)
- Entry `milp::solve(model, Options)` at src/milp/milp_solver.cpp:22, declared include/markov_cero/milp/milp_solver.hpp:47.
- Defaults (include/markov_cero/milp/milp_solver.hpp): `max_nodes 50000`, `max_queued_nodes 50000`, `max_iterations 500000`, `time_limit 60s`, relative gap 1e-4, absolute gap 1e-6, integrality 1e-6, feasibility 1e-7, `max_cut_rounds 5`, `separation_frequency 1`, `max_pool_cuts 40`, `max_pump_iterations 10`, `branching_strategy pseudo_cost`. Queue saturation preserves the weakest dropped-node bound and returns `ResourceLimit`.
- All-continuous models short-circuit to LP (or QP via `qp::solve_qp` when `has_quadratic_objective`) (src/milp/milp_solver.cpp:41-70).
- Root phase in order: root LP (src/milp/milp_solver.cpp:109) → [[PrimalHeuristics]] (`simple_rounding`, `feasibility_pump`) (src/milp/milp_solver.cpp:146-163) → GMI+MIR cuts and one root re-solve (src/milp/milp_solver.cpp:170-212) → root gap check (src/milp/milp_solver.cpp:213-229) → root strong branching to seed pseudo-costs and tighten bounds (src/milp/milp_solver.cpp:230-261).
- Tree is a `std::priority_queue` with `NodeCompareBestBound` (src/milp/milp_solver.cpp:264-266); loop stops on `max_nodes` or `time_limit_seconds` (src/milp/milp_solver.cpp:279-285); a hit sets `stop_reason` so the final status stays honest.
- Node LP via `solve_node_relaxation` (src/milp/node_lp.cpp:9): quadratic models use [[QP-ADMM-Engine]] (src/milp/node_lp.cpp:15-38); otherwise warm-started [[DualSimplexEngine]] or [[RevisedSimplexEngine]] (src/milp/node_lp.cpp:45-77).
- Bound pruning against `best_upper_bound − absolute_gap_tolerance` (src/milp/milp_solver.cpp:292, 324); pseudo-cost updates from child bound deltas (src/milp/milp_solver.cpp:328-339).
- Rounding heuristic runs every 5th explored node (src/milp/milp_solver.cpp:471-483).
- In-tree cut separation (RW-1, ED-005) at src/milp/milp_solver.cpp:341-443: frequency-gated GMI+MIR rounds, violation filter, cosine-similarity dedup against `root_cut_list` + inherited pool, failed re-solve reverts the round, pool trimmed to `max_pool_cuts`; children inherit `local_cuts` (src/milp/milp_solver.cpp:516, 539).
- Branching: strong if requested and a basis exists, else `select_branching_variable` (src/milp/milp_solver.cpp:451-479); down/up children tighten one bound each (src/milp/milp_solver.cpp:496-545).
- Relative-gap termination inside the loop; final assembly (src/milp/milp_solver.cpp:558-594) reports `Optimal` **only when proven** (`queue.empty() || gap_closed`), otherwise `ResourceLimit` with the stop reason.

## Dependencies
- [[DualSimplexEngine]], [[RevisedSimplexEngine]], [[QP-ADMM-Engine]], [[CutGenerators]], [[PrimalHeuristics]], [[Canonicalizer]]

## Used By
- [[Solve-Pipeline]] (`milp`/`miqp` engines, apps/markov_cero_solve.cpp:239-240), [[CLI-MarkovCeroSolve]]

## Research Justification
- [[Branch and Cut]], [[Branch and Bound]], [[Strong Branching]], [[Pseudo-Cost Branching]]

## Open Questions / Risks
- ~~`Options::node_strategy` declared but never read~~ — resolved: enum and option removed from include/markov_cero/milp/milp_solver.hpp; only best-bound order is implemented.
- ~~Cuts are generated only at the root~~ — resolved by RW-1: in-tree separation at src/milp/milp_solver.cpp:341 (see [[root-only-cuts]]).
- ~~Optimal status reported whenever any incumbent exists~~ — resolved: final status now requires `proven = queue.empty() || gap_closed`, else `ResourceLimit` (src/milp/milp_solver.cpp:568-594).
