---
type: concept
tags: [concepts, lp]
status: stable
verified_on: 2026-09-25
---

# Degeneracy

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> More columns want to enter than the vertex can move — the default state of industrial LPs and the reason pivot rules decide whether a solver converges.

## Definition
Primal degeneracy: a basic feasible solution in which at least one basic variable is zero, so the ratio test admits a zero-length step. Dual degeneracy: more than one nonbasic column has zero reduced cost at an optimum, so the objective does not distinguish pivot directions. A single vertex may then be represented by several bases (alternate optima), and the simplex can revisit bases forever (cycling) or stall without progress (stalling). Degeneracy arises from redundant/tight constraints, bound-tight rows and the unit/slack structure that MILP relaxations produce by construction.

## Why It Matters Here
- R13 names "highly degenerate models" explicitly and R9 demands reliable convergence on them; R17 asks for a demonstration, not a claim.
- Observed state: the reference engine defaults to Bland anti-cycling pricing and breaks ratio-test ties on the smallest basis index (src/lp/reference/revised_simplex.cpp:128-173); the dual engine defaults `harris_ratio` on (include/markov_cero/lp/dual/dual_simplex.hpp:32).
- Inference: tie-handling exists as a *correctness floor*, but no steepest-edge pricing or perturbation is present, so pivot quality on degenerate instances is unmeasured.

## Key Facts / Rules
- Degenerate pivot ⇒ step length θ* = 0 in the ratio test ⇒ objective unchanged, basis may repeat.
- Finite bases ⇒ cycling is possible; Bland's rule (smallest admissible index) guarantees no basis repeats ⇒ termination.
- Perturbation methods (Charnes 1954; Megiddo) break ties by slightly moving b/c and stay polynomially bounded.
- Near-degeneracy in floating point is indistinguishable from exact degeneracy — hence tolerance-based ratio tests.

## Related
- [[Bland Anti-Cycling]]
- [[Harris Ratio Test]]
- [[Ill-Conditioning]]
- [[Weak Relaxation]]
- [[Steepest Edge]]
- [[Bland-1977-Anti-Cycling-Rule]]

## Referenced By

- 15-roadmap
- 21-traceability
- Research MOC
- [[Steepest Edge|research/algorithms/Steepest Edge]]
- [[Basic Solution|research/concepts/Basic Solution]]
- [[Ill-Conditioning|research/concepts/Ill-Conditioning]]
- [[Bland-Only Pricing|research/limitations/Bland-Only Pricing]]
- cross-paper-synthesis
- [[Achterberg-2020-Presolve-Reductions-Mixed|research/papers/Achterberg-2020-Presolve-Reductions-Mixed]]
- [[Andersen-1995-Presolving-Linear-Programming|research/papers/Andersen-1995-Presolving-Linear-Programming]]
- [[Berthold-2013-Cloud-Branching|research/papers/Berthold-2013-Cloud-Branching]]
- [[Berthold-2023-Feasibility-Jump|research/papers/Berthold-2023-Feasibility-Jump]]
- [[Bland-1977-Anti-Cycling-Rule|research/papers/Bland-1977-Anti-Cycling-Rule]]
- [[Charnes-1954-Optimality-Multi-Valuedness|research/papers/Charnes-1954-Optimality-Multi-Valuedness]]
- [[Chinneck-1987-Primal-Dual-Methods|research/papers/Chinneck-1987-Primal-Dual-Methods]]
- [[Chinneck-1992-Feasibility-Redundancy-Linear|research/papers/Chinneck-1992-Feasibility-Redundancy-Linear]]
- [[Connell-1999-Dual-Active-Set-Algorithm|research/papers/Connell-1999-Dual-Active-Set-Algorithm]]
- [[Cook-n.d.-Space-Branching-Rules|research/papers/Cook-n.d.-Space-Branching-Rules]]
- [[DeFarias-2019-Positive-Edge-Pricing|research/papers/DeFarias-2019-Positive-Edge-Pricing]]
- [[Fischetti-2015-Improving-Branch-Cut|research/papers/Fischetti-2015-Improving-Branch-Cut]]
- [[Fourer-1994-Steepest-Edge-Simplexing|research/papers/Fourer-1994-Steepest-Edge-Simplexing]]
- [[Gill-1989-Practical-Anti-Cycling|research/papers/Gill-1989-Practical-Anti-Cycling]]
- [[Goldfarb-1992-Steepest-Edge-Simplex|research/papers/Goldfarb-1992-Steepest-Edge-Simplex]]
- [[Harris-1973-Pivot-Selection-Methods|research/papers/Harris-1973-Pivot-Selection-Methods]]
- [[Koberstein-2005-Dual-Simplex-Method|research/papers/Koberstein-2005-Dual-Simplex-Method]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector|research/papers/Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Maros-0000-New-Degeneracy-Method|research/papers/Maros-0000-New-Degeneracy-Method]]
- [[Maros-1993-Practical-Anti-Degeneracy|research/papers/Maros-1993-Practical-Anti-Degeneracy]]
- [[Maros-2003-Generalized-Dual-Phase|research/papers/Maros-2003-Generalized-Dual-Phase]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior|research/papers/Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Padberg-1991-Branch-and-Cut-Algorithm|research/papers/Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Renegar-1994-Condition-Numbers-Linear|research/papers/Renegar-1994-Condition-Numbers-Linear]]
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques|research/papers/Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Vanderbei-1995-Symmetric-Indefinite-Systems|research/papers/Vanderbei-1995-Symmetric-Indefinite-Systems]]
- [[Ye-1998-Crossover-Interior-Point|research/papers/Ye-1998-Crossover-Interior-Point]]
- [[Degeneracy Handling Gap|research/research-gaps/Degeneracy Handling Gap]]
- [[Ill-Conditioned Instance Dossier|research/research-gaps/Ill-Conditioned Instance Dossier]]
- [[Bland Anti-Cycling|research/techniques/Bland Anti-Cycling]]
- [[Harris Ratio Test|research/techniques/Harris Ratio Test]]