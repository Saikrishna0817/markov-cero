---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/milp/parallel_tree_search.cpp:220"
  - "include/markov_cero/milp/parallel_tree_search.hpp:28"
verified_on: 2026-09-25
---

# ParallelTreeSearch

> Multi-threaded branch-and-bound over a shared thread-safe queue using `std::jthread` and `std::stop_token`.

## Responsibility
- Run N workers that pop nodes from a shared best-bound heap, solve node LPs, publish incumbents and pseudo-costs, and cooperatively prune/stop.

## Implementation Facts (Observed)
- Entry `solve_parallel(model, ParallelOptions)` (include/markov_cero/milp/parallel_tree_search.hpp:28, src/milp/parallel_tree_search.cpp); `ParallelResult` is an alias of `milp::Result` (hpp:33).
- `ParallelOptions` defaults mirror `milp::Options`: `num_threads 4`, 60s, 50000 nodes, cuts/MIR/heuristics/strong-branching enabled, `branching_strategy pseudo_cost` (include/markov_cero/milp/parallel_tree_search.hpp:11-30).
- Workers are `std::jthread` objects started with a lambda taking `std::stop_token` (src/milp/parallel_tree_search.cpp:460-467); the worker loop exits on `stop_token.stop_requested() || queue.is_stopped()` (src/milp/parallel_tree_search.cpp:89).
- Stop conditions inside the loop: time/node limit (src/milp/parallel_tree_search.cpp:92-94) and relative gap closure (src/milp/parallel_tree_search.cpp:109-110).
- Shared state: `ThreadSafeNodeQueue` (mutex + condition_variable min-heap, include/markov_cero/milp/work_queue.hpp:18-57), `IncumbentManager` with atomic objective + mutex-protected primal (include/markov_cero/milp/shared_incumbent.hpp:13-30), and `SharedPseudoCosts` guarded by a mutex (src/milp/parallel_tree_search.cpp:70-73).
- Per-worker atomic bounds feed `compute_tree_lower_bound` (src/milp/parallel_tree_search.cpp:76-79, include/markov_cero/milp/shared_incumbent.hpp:34-36).
- Improving incumbents trigger queue pruning at `incumbent − absolute_gap_tolerance` (src/milp/parallel_tree_search.cpp:171-173, 185-188).
- Root work (LP, rounding, pump, GMI/MIR cuts, strong branching) happens **before** workers start (src/milp/parallel_tree_search.cpp:315-447).
- After join, status is optimal iff an incumbent exists; `best_bound` from `compute_tree_lower_bound` unless queue drained (src/milp/parallel_tree_search.cpp:477-488).

## Dependencies
- [[BranchAndCut]] (shared LP/cut/heuristic routines), [[PrimalHeuristics]], [[CutGenerators]], [[DualSimplexEngine]]

## Used By
- [[Solve-Pipeline]] (`--engine parallel`, apps/markov_cero_solve.cpp:92-135), [[CLI-MarkovCeroSolve]]

## Research Justification
- [[Branch and Bound]]

## Open Questions / Risks
- Inference: worker LPs share no basis-cache except warm start from the parent node, so parallel speedup depends heavily on LP cost.
- TSAN build option exists (`MARKOV_CERO_ENABLE_TSAN`, CMakeLists.txt:5) but race-test results are UNVERIFIED here.
- Root strong-branching results are copied into shared pseudo-costs under a lock (src/milp/parallel_tree_search.cpp:406-410); no other ordering guarantees documented.
