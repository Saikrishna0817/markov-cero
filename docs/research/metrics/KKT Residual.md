---
type: metric
tags: [metrics, numerics]
status: stable
verified_on: 2026-09-25
---

# KKT Residual

> The maximum violation of stationarity, feasibility and complementarity — one number that decides whether a solution may be called optimal.

## Definition
The KKT residual aggregates the violations of the Karush–Kuhn–Tucker conditions for the reported point: stationarity of the Lagrangian, primal feasibility (‖Ax − b‖, bound violations), dual feasibility (negative reduced costs) and complementary slackness (x_j·d_j ≠ 0). Practically it is the max (or a norm) of those components, each normalized by a scale factor so terms are comparable; a solution is "optimal to tolerance τ" when the residual ≤ τ. Because it is computed independently of the solver's internal status string, it is the natural quantity for a zero-trust verifier: the solver can claim anything, the residual is checkable arithmetic.

## Why It Matters Here
- R17 (demonstrate numerical robustness) and R20 (solution quality) are both graded through verifiable residuals.
- Observed state: independent verifiers exist (`reference_lp_verifier`, `primal_verifier`, zero-trust KKT verifier, docs/codebase/components/IndependentVerifiers.md) and every terminal reference result is certified via `verify::verify_reference_result` (src/lp/reference/revised_simplex.cpp:357).
- Observed state: PDLP reports `optimal` only when primal, dual **and** gap residuals are ≤ tolerance (src/lp/first_order/pdlp.cpp:333-346); ADMM uses absolute+relative residual bounds (src/qp/admm_solver.cpp:200-210).

## Key Facts / Rules
- Components: stationarity ‖c − Aᵀπ − …‖ (with active-set/complementarity terms), primal feasibility, dual feasibility, complementarity max_j x_j·d_j.
- Normalize each component by a data scale (‖c‖, ‖b‖, max(1,|objective|)) before taking the max.
- Residual ≤ tolerance certifies a *near-KKT* point; exact optimality certificates (Farkas, dual bounds) are stronger.
- Residuals are scale-sensitive: an unscaled model can hide violations that matter.

## Related
- [[KKT Conditions]]
- [[Primal Residual]]
- [[Numerical Error]]
- [[Iterative Refinement]]
- [[IndependentVerifiers]]

## Referenced By

- [[16-testing-evaluation-strategy|audit/16-testing-evaluation-strategy]]
- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Evaluation MOC|research/Evaluation MOC]]
- [[Research MOC|research/Research MOC]]
- [[Iterative Refinement|research/algorithms/Iterative Refinement]]
- [[KKT Conditions|research/concepts/KKT Conditions]]
- [[Netlib LP Collection|research/datasets/Netlib LP Collection]]
- [[ED-008-retain-zero-trust-verifiers|research/engineering-decisions/ED-008-retain-zero-trust-verifiers]]
- [[First-Order Accuracy Ceiling|research/limitations/First-Order Accuracy Ceiling]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Numerical Error|research/metrics/Numerical Error]]
- [[Primal Residual|research/metrics/Primal Residual]]
- [[Relative Optimality Gap|research/metrics/Relative Optimality Gap]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics|research/papers/Achterberg-2011-Rounding-Propagation-Heuristics]]
- [[Berthold-2006-Primal-Heuristics-Mixed|research/papers/Berthold-2006-Primal-Heuristics-Mixed]]
- [[Boggs-1995-Sequential-Quadratic-Programming|research/papers/Boggs-1995-Sequential-Quadratic-Programming]]
- [[Cline-1979-Estimate-Condition-Number|research/papers/Cline-1979-Estimate-Condition-Number]]
- [[Gill-1989-Practical-Anti-Cycling|research/papers/Gill-1989-Practical-Anti-Cycling]]
- [[Gleixner-2015-Iterative-Refinement-Linear|research/papers/Gleixner-2015-Iterative-Refinement-Linear]]
- [[Goldfarb-1983-Numerically-Stable-Dual|research/papers/Goldfarb-1983-Numerically-Stable-Dual]]
- [[Goldfarb-1984-Dual-Primal-Dual-Methods|research/papers/Goldfarb-1984-Dual-Primal-Dual-Methods]]
- [[Gomory-1963-All-Integer-Programming|research/papers/Gomory-1963-All-Integer-Programming]]
- [[Gondzio-1996-Multiple-Centrality-Corrections|research/papers/Gondzio-1996-Multiple-Centrality-Corrections]]
- [[Karmarkar-1984-New-Polynomial-Time-Algorithm|research/papers/Karmarkar-1984-New-Polynomial-Time-Algorithm]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm|research/papers/Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Lu-2025-cuPDLP-GPU-Implementation|research/papers/Lu-2025-cuPDLP-GPU-Implementation]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector|research/papers/Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[McShane-1989-Implementation-Primal-Dual-Interior|research/papers/McShane-1989-Implementation-Primal-Dual-Interior]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior|research/papers/Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Moler-1967-Rounding-Errors-Algebraic|research/papers/Moler-1967-Rounding-Errors-Algebraic]]
- [[Padberg-2005-Classical-Cuts-Mixed|research/papers/Padberg-2005-Classical-Cuts-Mixed]]
- [[Renegar-1994-Condition-Numbers-Linear|research/papers/Renegar-1994-Condition-Numbers-Linear]]
- [[Steinrucken-2019-Exact-Algorithms-Linear|research/papers/Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Tits-1994-Simple-Quadratically-Convergent|research/papers/Tits-1994-Simple-Quadratically-Convergent]]
- [[Unknown-2025-Overview-GPU-Based-First|research/papers/Unknown-2025-Overview-GPU-Based-First]]
- [[Unknown-2026-Verified-Linear-Programming|research/papers/Unknown-2026-Verified-Linear-Programming]]
- [[Vanderbei-1995-Symmetric-Indefinite-Systems|research/papers/Vanderbei-1995-Symmetric-Indefinite-Systems]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic|research/papers/Wilkinson-1963-Rounding-Errors-Algebraic]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods|research/papers/Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Ye-1991-Interior-Point-Algorithm|research/papers/Ye-1991-Interior-Point-Algorithm]]
- [[Ill-Conditioned Instance Dossier|research/research-gaps/Ill-Conditioned Instance Dossier]]