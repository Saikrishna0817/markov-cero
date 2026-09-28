---
type: limitation
tags: [limitations, numerics]
status: stable
verified_on: 2026-09-25
---

# First-Order Accuracy Ceiling

> First-order methods converge to a tolerance floor and stop — no basis, no extra digits, no refinement pass to push further.

## Definition
PDHG/PDLP and ADMM are first-order methods: their per-iteration cost is one matrix-vector product plus cheap vector ops, but their convergence rate is O(1/k) (ergodic), so residual reduction slows dramatically as tolerance tightens. Reaching high accuracy therefore costs many iterations, and beyond a certain point residual *stagnation* from rounding and conditioning sets in — the accuracy ceiling. Unlike factorization-based methods, there is no built-in mechanism (basis, refinement, extended precision) to recover lost digits after the fact; the only levers are more iterations, better preconditioning, or a different algorithm class (IPM/crossover/simplex with refinement).

## Why It Matters Here
- R9/R17: "numerical robustness" claims cannot rest on a method whose certified accuracy is tolerance-bounded by design.
- Observed state: PDLP declares `optimal` only when all residuals ≤ tolerance (default CLI iteration limit 100000) (src/lp/first_order/pdlp.cpp:333-411); ADMM tolerances are 1e-4 with a first-order residual floor (include/markov_cero/qp/admm_solver.hpp:25-38).
- Observed state: no iterative refinement exists anywhere in the repo — Inference: accuracy claims for first-order paths are bounded by their stopping tolerances, not by a verified error bound.

## Key Facts / Rules
- Rate: ergodic O(1/k) ⇒ halving the residual costs roughly a doubling of iterations.
- Stagnation floor ≈ conditioning × ε × problem scale; preconditioning (Ruiz, diagonal) lowers but does not remove it.
- Certificates are the workaround: report the measured KKT residual, not an asserted accuracy.
- Escalation path: crossover to simplex for a basis, then refinement for digits — neither exists here.

## Related
- [[Primal-Dual Hybrid Gradient]]
- [[ADMM]]
- [[Iterative Refinement]]
- [[KKT Residual]]
- [[Numerical Error]]
- [[No Interior-Point Engine]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Iterative Refinement|research/algorithms/Iterative Refinement]]
- [[Primal-Dual Hybrid Gradient|research/algorithms/Primal-Dual Hybrid Gradient]]
- [[ED-003-interior-point-required-by-ps|research/engineering-decisions/ED-003-interior-point-required-by-ps]]
- [[ED-007-honest-gpu-scoping|research/engineering-decisions/ED-007-honest-gpu-scoping]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Numerical Error|research/metrics/Numerical Error]]