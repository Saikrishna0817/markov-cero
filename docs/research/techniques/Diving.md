---
type: concept
tags: [techniques, milp]
status: stable
verified_on: 2026-09-25
---

# Diving

> Fix one integer variable at a time from the LP point and re-solve — a greedy descent toward a feasible incumbent without touching the tree.

## Definition
A dive starts from a fractional LP solution and repeatedly selects a fractional variable (by pseudo-cost, fractionality or a rule like "fix the most fractional"), fixes it to its rounded value in a *temporary* copy of the model, re-solves the LP, and continues until the point is integer-feasible or a violation forces a backtrack/restart. It is a primal heuristic — the fixes are discarded afterwards and never constrain the real tree — and it explores one path deeply rather than many nodes shallowly. Variant rules (best projection, three-way branching with abstention, objective diving which also improves the objective) trade success rate against LP re-solve count.

## Why It Matters Here
- R5 lists heuristics among required components; diving is the standard second primal heuristic after rounding and before local search.
- Observed state: **not implemented** — src/milp/heuristics.cpp exposes only `simple_rounding` and `feasibility_pump`; docs/codebase/components/PrimalHeuristics.md documents the two-strategy set with no dive path.
- Inference: adding a dive is bounded work (the LP re-solve + warm-start machinery already exists) and would close part of the primal-heuristic thinness flagged in the audit's R5 row.

## Key Facts / Rules
- Each iteration = fix one variable + one LP re-solve (warm-started from the parent basis — cheap by design).
- Selection rules: most-fractional, pseudo-cost based (align with [[Pseudo-Cost Branching]]), or best projection onto the current LP point.
- Failure handling: backtrack k fixes, flip the last decision, or abandon the dive; the tree is unaffected either way.
- Interaction with cuts: a dive re-solves an LP repeatedly, so it inherits the same tolerance/scale sensitivity as the engine.

## Related
- [[Rounding Heuristic]]
- [[Feasibility Pump]]
- [[Pseudo-Cost Branching]]
- [[LP Relaxation]]
- [[PrimalHeuristics]]
- [[Berthold-2007-Heuristics-Branch-Cut]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[Research MOC|research/Research MOC]]
- [[Feasibility Pump|research/algorithms/Feasibility Pump]]
- [[Pseudo-Cost Branching|research/algorithms/Pseudo-Cost Branching]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Achterberg-0000-Objective-Feasibility-Pump|research/papers/Achterberg-0000-Objective-Feasibility-Pump]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics|research/papers/Achterberg-2011-Rounding-Propagation-Heuristics]]
- [[Berthold-2006-Primal-Heuristics-Mixed|research/papers/Berthold-2006-Primal-Heuristics-Mixed]]
- [[Berthold-2007-Heuristics-Branch-Cut|research/papers/Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2007-RENS-Relaxation-Enforced|research/papers/Berthold-2007-RENS-Relaxation-Enforced]]
- [[Berthold-2025-Primal-Heuristics-Mixed|research/papers/Berthold-2025-Primal-Heuristics-Mixed]]
- [[Fischetti-2003-Local-Branching|research/papers/Fischetti-2003-Local-Branching]]
- [[Spoorendonk-2026-Presolve-Heuristics-HiGHS|research/papers/Spoorendonk-2026-Presolve-Heuristics-HiGHS]]
- [[Rounding Heuristic|research/techniques/Rounding Heuristic]]