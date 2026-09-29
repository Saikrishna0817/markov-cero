---
type: concept
tags: [techniques, milp]
status: stable
verified_on: 2026-09-25
---

# Rounding Heuristic

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Round the fractional LP point to integers and check — the cheapest possible source of incumbents.

## Definition
A rounding heuristic takes the LP relaxation's optimal point and maps it to an integer candidate, either by nearest-integer rounding, by objective-directed rounding (floor or ceil per variable according to the sign of its cost), or by a randomized/iterated variant, then checks feasibility against the original constraints and bounds. Its success rate is a direct readout of how tight the relaxation is: when the relaxation is weak, plain rounding almost never lands feasible, and the heuristic degenerates to generating candidates that must be repaired (which is what the feasibility pump's projection step does). Rounding is deterministic, O(n), and requires no LP re-solve, so it is worth running frequently inside the tree.

## Why It Matters Here
- R5 requires heuristics; incumbents prune siblings, so each cheap incumbent directly reduces node count and R20 runtime.
- Observed state: `simple_rounding` tries three deterministic strategies in order — nearest-integer, objective-directed floor/ceil by cost sign, all-up — each clamped to variable bounds (src/milp/heuristics.cpp:89-147); invoked at the root and every 5th explored node (src/milp/milp_solver.cpp:147, 346-358).
- Observed state: propagation-based rounding and dives are absent (only rounding + feasibility pump exist) — Inference: the primal side is thin relative to the Berthold/Achterberg taxonomy.

## Key Facts / Rules
- Strategies: nearest-integer; sign-of-cost directed floor/ceil; all-up; then feasibility check in original space.
- Always clamp to bounds before feasibility check — rounding can violate a tight box constraint trivially.
- Success probability rises with relaxation tightness; failure is *not* evidence of infeasibility.
- Repair variants (round then fix-and-propagate) are the natural upgrade path.

## Related
- [[LP Relaxation]]
- [[Feasibility Pump]]
- [[Diving]]
- PrimalHeuristics
- [[Achterberg-2011-Rounding-Propagation-Heuristics]]

## Referenced By

- 15-roadmap
- Research MOC
- [[Feasibility Pump|research/algorithms/Feasibility Pump]]
- [[LP Relaxation|research/concepts/LP Relaxation]]
- cross-paper-synthesis
- [[Achterberg-2007-Improving-Feasibility-Pump|research/papers/Achterberg-2007-Improving-Feasibility-Pump]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics|research/papers/Achterberg-2011-Rounding-Propagation-Heuristics]]
- [[Applegate-2006-Traveling-Salesman-Problem|research/papers/Applegate-2006-Traveling-Salesman-Problem]]
- [[Berthold-2006-Primal-Heuristics-Mixed|research/papers/Berthold-2006-Primal-Heuristics-Mixed]]
- [[Berthold-2007-Heuristics-Branch-Cut|research/papers/Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2007-RENS-Relaxation-Enforced|research/papers/Berthold-2007-RENS-Relaxation-Enforced]]
- [[Berthold-2023-Feasibility-Jump|research/papers/Berthold-2023-Feasibility-Jump]]
- [[Berthold-2025-Primal-Heuristics-Mixed|research/papers/Berthold-2025-Primal-Heuristics-Mixed]]
- [[Danna-2004-Exploring-Relaxation-Induced|research/papers/Danna-2004-Exploring-Relaxation-Induced]]
- [[Fischetti-2003-Local-Branching|research/papers/Fischetti-2003-Local-Branching]]
- [[Fischetti-2005-Feasibility-Pump|research/papers/Fischetti-2005-Feasibility-Pump]]
- [[Fischetti-2006-Feasibility-Pump-Heuristic|research/papers/Fischetti-2006-Feasibility-Pump-Heuristic]]
- [[Hillier-1969-Efficient-Heuristic-Procedures|research/papers/Hillier-1969-Efficient-Heuristic-Procedures]]
- [[Mexi-2026-Frank-Wolfe-based-Primal|research/papers/Mexi-2026-Frank-Wolfe-based-Primal]]
- [[Spoorendonk-2026-Presolve-Heuristics-HiGHS|research/papers/Spoorendonk-2026-Presolve-Heuristics-HiGHS]]
- [[Diving|research/techniques/Diving]]