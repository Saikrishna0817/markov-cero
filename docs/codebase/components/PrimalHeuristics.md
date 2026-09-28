---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/milp/heuristics.cpp:81"
  - "src/milp/heuristics.cpp:152"
verified_on: 2026-09-25
---

# PrimalHeuristics

> Simple rounding strategies and a cycling-aware feasibility pump for MILP incumbents.

## Responsibility
- Turn a fractional LP point into integer candidates quickly, and when that fails iterate round → distance-minimization LP → round until an incumbent appears.

## Implementation Facts (Observed)
- API in include/markov_cero/milp/heuristics.hpp: `check_integer_feasibility` (hpp:17), `compute_objective` (hpp:22), `simple_rounding` (hpp:25), `feasibility_pump(model, x, max_iterations=10, ...)` (hpp:30-34).
- `simple_rounding` tries three deterministic strategies in order: nearest-integer (src/milp/heuristics.cpp:99-111), objective-directed floor/ceil by cost sign (src/milp/heuristics.cpp:113-133), all-up rounding (src/milp/heuristics.cpp:135-147); each is clamped to variable bounds (src/milp/heuristics.cpp:89-97).
- `feasibility_pump` rounds integers first and returns immediately if feasible (src/milp/heuristics.cpp:172-194).
- Cycling detection compares against all previously rounded points and, on a cycle, flips the 1..3 variables closest to fraction 0.5 — comment cites Fischetti, Lodi, Glover 2005 (src/milp/heuristics.cpp:196-249).
- Each iteration builds a distance objective `min Σ| x_j − x̃_j |` as sign-coded linear costs (src/milp/heuristics.cpp:252-276), canonicalizes with `relax_integrality=true`, and solves with [[RevisedSimplexEngine]] capped at 5000 iterations (src/milp/heuristics.cpp:278-288).
- Any LP failure aborts the pump silently via `catch (...) { break; }` (src/milp/heuristics.cpp:285-291).
- Invocation: root pump once (src/milp/milp_solver.cpp:156-162, `max_pump_iterations` default 10), root rounding (src/milp/milp_solver.cpp:147), and in-tree rounding every 5 nodes (src/milp/milp_solver.cpp:346).

## Dependencies
- [[Canonicalizer]], [[RevisedSimplexEngine]]

## Used By
- [[BranchAndCut]], [[ParallelTreeSearch]] (src/milp/parallel_tree_search.cpp:183, 315-326), [[CLI-MarkovCeroSolve]] (`--heuristics/--no-heuristics`)

## Research Justification
- [[Feasibility Pump]]

## Open Questions / Risks
- Pump uses only linear distance costs — no objective-proximity term, so Inference: it may converge to feasible but poor incumbents.
- Result reporting is binary `found`; no iteration-count/telemetry is returned (include/markov_cero/milp/heuristics.hpp:11-15).
