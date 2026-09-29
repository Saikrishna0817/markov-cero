---
type: concept
tags: [concepts, lp]
status: stable
verified_on: 2026-09-25
---

# Duality Gap

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The distance between what the primal achieves and what the dual proves — every "optimality" claim in this repo is a statement about this number.

## Definition
For a primal feasible x and dual feasible π, weak duality gives cᵀx − bᵀπ ≥ 0 (minimization); the difference is the duality gap. Linear-programming strong duality states the gap is exactly zero when both sides are feasible and bounded, so a zero gap certifies optimality. In a MIP, the analogous gap is between the incumbent (best integer solution) and the LP-bound tree bound; it never closes by itself below the integrality gap of the formulation. Relative gap normalizes by the incumbent magnitude so instances of different scale compare.

## Why It Matters Here
- R20 asks for "optimal or near-optimal" results; the gap is the only honest way to say "near-optimal".
- Observed state: MILP termination defaults relative gap 1e-4 / absolute gap 1e-6 (include/markov_cero/milp/milp_solver.hpp:16-31) and pruning uses `best_upper_bound − absolute_gap_tolerance` (src/milp/milp_solver.cpp:287).
- Observed state: PDLP reports `optimal` only when primal, dual and gap residuals are all within tolerance (src/lp/first_order/pdlp.cpp:333-346).

## Key Facts / Rules
- Weak duality: gap ≥ 0 for any feasible pair; gap = 0 ⇔ both solutions optimal (LP).
- Phase-I with a positive optimal value yields a Farkas certificate of infeasibility instead of a gap (src/lp/reference/revised_simplex.cpp:426-434).
- MIP relative gap ≈ (UB − LB)/max(|UB|, small ε); UB from incumbents, LB from the open-node bound.
- Duality gap is measured in objective units; it is *not* the same as primal/dual feasibility residuals.

## Related
- [[KKT Conditions]]
- [[LP Relaxation]]
- [[Relative Optimality Gap]]
- [[Reduced Cost]]
- [[Branch and Bound]]

## Referenced By

- [[Basic Solution|research/concepts/Basic Solution]]
- [[KKT Conditions|research/concepts/KKT Conditions]]
- [[Reduced Cost|research/concepts/Reduced Cost]]
- [[Primal Residual|research/metrics/Primal Residual]]
- [[Relative Optimality Gap|research/metrics/Relative Optimality Gap]]
- [[Bixby-1994-Reduced-Cost-Fixing|research/papers/Bixby-1994-Reduced-Cost-Fixing]]
- [[Boggs-1995-Sequential-Quadratic-Programming|research/papers/Boggs-1995-Sequential-Quadratic-Programming]]
- [[Dantzig-1963-Linear-Programming-Extensions|research/papers/Dantzig-1963-Linear-Programming-Extensions]]
- [[Gill-0000-Two-Phase-Algorithms|research/papers/Gill-0000-Two-Phase-Algorithms]]
- [[Goldfarb-1983-Numerically-Stable-Dual|research/papers/Goldfarb-1983-Numerically-Stable-Dual]]
- [[Goldfarb-1984-Dual-Primal-Dual-Methods|research/papers/Goldfarb-1984-Dual-Primal-Dual-Methods]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm|research/papers/Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Renegar-1994-Condition-Numbers-Linear|research/papers/Renegar-1994-Condition-Numbers-Linear]]
- [[Tits-1994-Simple-Quadratically-Convergent|research/papers/Tits-1994-Simple-Quadratically-Convergent]]
- [[Ye-1991-Interior-Point-Algorithm|research/papers/Ye-1991-Interior-Point-Algorithm]]
- [[Adaptive Restart|research/techniques/Adaptive Restart]]