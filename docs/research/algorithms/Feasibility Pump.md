---
type: concept
tags: [algorithms, milp]
status: stable
verified_on: 2026-09-25
---

# Feasibility Pump

> Round, project back onto the constraints, round again — the fastest known route to a first MIP incumbent.

## Definition
The feasibility pump alternates between two projections: round the current LP point to an integer candidate T(x̃), then solve an LP that projects back to the relaxed feasible set, minimizing a distance objective ‖x − x̃‖₁ (implemented as sign-coded linear costs). Iterating this map converges quickly to an integer-feasible point when one exists nearby, and it needs no branching — the projection LP is a single small LP solve per iteration. Failure modes are cycling between two candidates (mitigated by perturbing/flipping the variables closest to 0.5) and converging to a feasible point with a poor objective (fixed by adding an objective-proximity term, "objective feasibility pump").

## Why It Matters Here
- R5 requires heuristics; incumbents prune the tree, so time-to-first-incumbent is a real lever on R20.
- Observed state: `feasibility_pump(model, x, max_iterations=10)` with cycling detection that flips the 1..3 variables nearest fraction 0.5, citing Fischetti, Lodi & Glover 2005; each iteration builds the distance LP and solves it with the reference engine capped at 5000 iterations; invoked once at the root (src/milp/heuristics.cpp:172-291; src/milp/milp_solver.cpp:156-162).
- Inference (open question in docs/codebase/components/PrimalHeuristics.md): the distance objective has no objective-proximity term, so incumbents found may be feasible but weak.

## Key Facts / Rules
- Map: x̃ ← T(x_LP) (rounding), then x_LP ← argmin ‖x − x̃‖₁ s.t. LP constraints (projection).
- Cycling: detect repeats and flip the most-fractional-rounded variables.
- Objective FP (Achterberg & Berthold 2007) blends distance and objective terms — reported to roughly halve average gap.
- Pump is a *heuristic*: failure to converge proves nothing about infeasibility.

## Related
- [[Rounding Heuristic]]
- [[LP Relaxation]]
- [[Diving]]
- [[PrimalHeuristics]]
- [[Fischetti-2005-Feasibility-Pump]]
- [[Achterberg-2007-Improving-Feasibility-Pump]]

## Referenced By

- [[PrimalHeuristics|codebase/components/PrimalHeuristics]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Achterberg-0000-Objective-Feasibility-Pump|research/papers/Achterberg-0000-Objective-Feasibility-Pump]]
- [[Achterberg-2007-Improving-Feasibility-Pump|research/papers/Achterberg-2007-Improving-Feasibility-Pump]]
- [[Berthold-2006-Primal-Heuristics-Mixed|research/papers/Berthold-2006-Primal-Heuristics-Mixed]]
- [[Berthold-2007-Heuristics-Branch-Cut|research/papers/Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2023-Feasibility-Jump|research/papers/Berthold-2023-Feasibility-Jump]]
- [[Berthold-2025-Primal-Heuristics-Mixed|research/papers/Berthold-2025-Primal-Heuristics-Mixed]]
- [[Fischetti-2005-Feasibility-Pump|research/papers/Fischetti-2005-Feasibility-Pump]]
- [[Fischetti-2006-Feasibility-Pump-Heuristic|research/papers/Fischetti-2006-Feasibility-Pump-Heuristic]]
- [[Hillier-1969-Efficient-Heuristic-Procedures|research/papers/Hillier-1969-Efficient-Heuristic-Procedures]]
- [[Mexi-2026-Frank-Wolfe-based-Primal|research/papers/Mexi-2026-Frank-Wolfe-based-Primal]]
- [[Spoorendonk-2026-Presolve-Heuristics-HiGHS|research/papers/Spoorendonk-2026-Presolve-Heuristics-HiGHS]]
- [[Diving|research/techniques/Diving]]
- [[Rounding Heuristic|research/techniques/Rounding Heuristic]]