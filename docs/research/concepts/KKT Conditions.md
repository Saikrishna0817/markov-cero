---
type: concept
tags: [concepts, qp]
status: stable
verified_on: 2026-09-25
---

# KKT Conditions

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The single contract every engine's answer must satisfy — stationarity, feasibility, complementarity — and the shape of the independent verifiers.

## Definition
For a convex program with affine constraints, the Karush–Kuhn–Tucker conditions are necessary and sufficient for optimality: stationarity of the Lagrangian, primal feasibility, dual feasibility, and complementary slackness. For the LP min cᵀx s.t. Ax = b, x ≥ 0 they read Ax = b, x ≥ 0, Aᵀπ ≤ c, and xᵀ(c − Aᵀπ) = 0 — i.e. reduced costs nonnegative and zero only where x > 0. For a QP min ½xᵀPx + qᵀx they add P x + q + Aᵀy = 0 with the same complementarity, which is why QP solvers factor the KKT system directly. Linear/convex-quadratic constraints need no separate constraint qualification for the equivalence to hold.

## Why It Matters Here
- R2 (QP scope) and R9/R17 (numerical robustness) are all judged through KKT satisfaction, not through self-reported status strings.
- Observed state: independent verifiers exist (`reference_lp_verifier`, `primal_verifier`, zero-trust KKT verifier) and every terminal reference result passes `verify::verify_reference_result` (src/lp/reference/revised_simplex.cpp:357).
- Observed state: the ADMM engine returns primal/dual infeasibility certificates checked every 10 iterations (src/qp/admm_solver.cpp:212-280).

## Key Facts / Rules
- LP form: stationarity ⇔ d = c − Aᵀπ ≥ 0; complementarity ⇔ xⱼdⱼ = 0 for every j.
- QP form: P x + q + Aᵀ y = 0 restricted to active constraints (active-set/KKT view).
- KKT residual = max violation of those clauses; it is the standard stopping/certification quantity.
- KKT is necessary-and-sufficient here only because P ⪰ 0 and constraints are affine; nonconvex QP loses sufficiency.

## Related
- [[Reduced Cost]]
- [[Duality Gap]]
- [[KKT Residual]]
- [[ADMM]]
- IndependentVerifiers

## Referenced By

- Research MOC
- [[ADMM|research/algorithms/ADMM]]
- [[Interior-Point Method|research/algorithms/Interior-Point Method]]
- [[Sparse LDL Factorization|research/algorithms/Sparse LDL Factorization]]
- [[Duality Gap|research/concepts/Duality Gap]]
- [[Reduced Cost|research/concepts/Reduced Cost]]
- [[QPLIB|research/datasets/QPLIB]]
- [[KKT Residual|research/metrics/KKT Residual]]
- [[Boggs-1995-Sequential-Quadratic-Programming|research/papers/Boggs-1995-Sequential-Quadratic-Programming]]
- [[Connell-1999-Dual-Active-Set-Algorithm|research/papers/Connell-1999-Dual-Active-Set-Algorithm]]
- [[Gill-0000-Two-Phase-Algorithms|research/papers/Gill-0000-Two-Phase-Algorithms]]
- [[Goldfarb-1983-Numerically-Stable-Dual|research/papers/Goldfarb-1983-Numerically-Stable-Dual]]
- [[Goldfarb-1984-Dual-Primal-Dual-Methods|research/papers/Goldfarb-1984-Dual-Primal-Dual-Methods]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm|research/papers/Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Mexi-2026-Frank-Wolfe-based-Primal|research/papers/Mexi-2026-Frank-Wolfe-based-Primal]]
- [[Renegar-1994-Condition-Numbers-Linear|research/papers/Renegar-1994-Condition-Numbers-Linear]]
- [[Vanderbei-1996-Foundations-and-Extensions|research/papers/Vanderbei-1996-Foundations-and-Extensions]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods|research/papers/Wright-1997-Primal-Dual-Interior-Point-Methods]]