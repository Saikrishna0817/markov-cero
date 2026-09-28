---
type: concept
tags: [concepts, lp]
status: stable
verified_on: 2026-09-25
---

# Reduced Cost

> d = c − πᵀA — the marginal price of one unit of a column, and the simplex's optimality test in one line.

## Definition
Given dual prices π (row multipliers) for the canonical LP min cᵀx s.t. Ax = b, x ≥ 0, the reduced cost of column j is dⱼ = cⱼ − πᵀAⱼ, i.e. the objective change per unit of xⱼ after accounting for what its resources are worth through the duals. At a primal- and dual-feasible point, dⱼ ≥ 0 for minimization and dⱼ = 0 for every basic (positive) variable — this is complementary slackness expressed in pricing terms. Primal simplex picks a column with dⱼ < 0 to enter; dual simplex (in its dual view) prices columns the same way when testing optimality. Reduced costs also give a *fixing* rule: with dual feasibility, any column with dⱼ > 0 can be fixed at its lower bound in all optima.

## Why It Matters Here
- R4 (revised simplex) makes pricing the inner loop of the primary engine; R5 needs cheap bound tightening in MIP.
- Observed state: pricing is "first negative reduced cost" under `bland_anti_cycling` (default true) else most-negative (src/lp/reference/revised_simplex.cpp:128-151); the dual engine's `tableau_norm` policy is explicitly documented as *not* conventional dual steepest-edge (include/markov_cero/lp/dual/dual_simplex.hpp:12-14).
- Inference: the presolve pass implements only four reductions (empty row/column, row singleton, fixed variable — src/presolve/presolve.cpp:67-177), so reduced-cost fixing is available in the literature but not yet in the stack.

## Key Facts / Rules
- d = c − Aᵀπ (equivalently dⱼ = cⱼ − πᵀAⱼ); optimality ⇔ dⱼ ≥ 0 ∀j (minimization, x ≥ 0).
- Reduced-cost fixing: if dⱼ > 0 then xⱼ = lower bound at every optimum (requires dual feasibility).
- Pricing rules differ only in *which* negative column they pick; all preserve finite termination only if a fallback exists.
- Steepest edge ranks dⱼ by the unit step length of the entering column, not by dⱼ alone.

## Related
- [[Basis]]
- [[KKT Conditions]]
- [[Duality Gap]]
- [[Steepest Edge]]
- [[Bixby-1994-Reduced-Cost-Fixing]]

## Referenced By

- [[Dual Simplex|research/algorithms/Dual Simplex]]
- [[Revised Simplex|research/algorithms/Revised Simplex]]
- [[Steepest Edge|research/algorithms/Steepest Edge]]
- [[Duality Gap|research/concepts/Duality Gap]]
- [[KKT Conditions|research/concepts/KKT Conditions]]
- [[Bland-Only Pricing|research/limitations/Bland-Only Pricing]]
- [[Achterberg-2020-Presolve-Reductions-Mixed|research/papers/Achterberg-2020-Presolve-Reductions-Mixed]]
- [[Andersen-1995-Presolving-Linear-Programming|research/papers/Andersen-1995-Presolving-Linear-Programming]]
- [[Bixby-1994-Reduced-Cost-Fixing|research/papers/Bixby-1994-Reduced-Cost-Fixing]]
- [[Brearley-1975-Analysis-Mathematical-Programming|research/papers/Brearley-1975-Analysis-Mathematical-Programming]]
- [[DeFarias-2019-Positive-Edge-Pricing|research/papers/DeFarias-2019-Positive-Edge-Pricing]]
- [[Goldfarb-1977-Practicable-Steepest-Edge|research/papers/Goldfarb-1977-Practicable-Steepest-Edge]]
- [[Goldfarb-1992-Steepest-Edge-Simplex|research/papers/Goldfarb-1992-Steepest-Edge-Simplex]]
- [[Hillier-1969-Efficient-Heuristic-Procedures|research/papers/Hillier-1969-Efficient-Heuristic-Procedures]]
- [[Vanderbei-1996-Foundations-and-Extensions|research/papers/Vanderbei-1996-Foundations-and-Extensions]]
- [[Wang-2026-Enhancing-Presolve-Mixed|research/papers/Wang-2026-Enhancing-Presolve-Mixed]]