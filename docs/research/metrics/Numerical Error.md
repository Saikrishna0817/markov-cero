---
type: metric
tags: [metrics, numerics]
status: stable
verified_on: 2026-09-25
---

# Numerical Error

> Distance from the true answer — objective deviation, residual size, and the accuracy ceiling each engine can reach.

## Definition
Numerical error quantifies how far a computed solution is from the exact one, measured either *a posteriori* (KKT/feasibility residuals, which are computable without knowing the answer) or *a priori* against a trusted reference (exact-rational solve, higher-precision recomputation, or a benchmark's recorded optimum). For LP the practical proxies are the relative objective error |cᵀx − c*|/|c*| and the constraint violation ‖Ax − b‖; error grows like κ·ε for stable methods and like growth-factor effects for unstable ones. The distinction matters: a small residual certifies a near-KKT point even if the objective differs (weak duality bounds the gap), while a matching objective with large residuals certifies nothing.

## Why It Matters Here
- R17 (numerical robustness demonstration) and R9 (numerical stability emphasis) are graded on this quantity.
- Observed state: Netlib runs report relative error ≤ 7.9e-15 on 7 instances (evidence/netlib_results.csv, cited in docs/audit/00-ground-truth.md) — the strongest accuracy evidence currently in the repo; `SparseLu` tracks growth factor and pivot ranges (src/linalg/sparse_basis.cpp:140-143).
- Inference: without iterative refinement or extended-precision recomputation, error cannot be pushed below the κ·ε floor — the substance of the [[First-Order Accuracy Ceiling]] and part of the [[Ill-Conditioned Instance Dossier]] gap.

## Key Facts / Rules
- A posteriori: residuals (checkable); a priori: reference objective/point (needs a trusted reference).
- Stable solve: relative error ≈ κ(B)·ε; unstable pivoting: bounded by growth factor instead.
- First-order methods: error decreases with iterations and stalls at the tolerance/conditioning floor — refining them requires extra work (crossover/refinement), not more iterations.
- Report error *and* the residual, plus the tolerance and scaling used — absolute numbers are meaningless without them.

## Related
- [[Ill-Conditioning]]
- [[Iterative Refinement]]
- [[KKT Residual]]
- [[Numerical Stability]]
- [[First-Order Accuracy Ceiling]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic]]

## Referenced By

- [[numerical-policy-centralized|codebase/decisions/numerical-policy-centralized]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Evaluation MOC|research/Evaluation MOC]]
- [[Iterative Refinement|research/algorithms/Iterative Refinement]]
- [[Numerical Stability|research/concepts/Numerical Stability]]
- [[ED-008-retain-zero-trust-verifiers|research/engineering-decisions/ED-008-retain-zero-trust-verifiers]]
- [[First-Order Accuracy Ceiling|research/limitations/First-Order Accuracy Ceiling]]
- [[KKT Residual|research/metrics/KKT Residual]]
- [[Bartels-1968-Numerical-Investigation-Simplex|research/papers/Bartels-1968-Numerical-Investigation-Simplex]]
- [[Gartner-1999-Exact-Arithmetic-Low|research/papers/Gartner-1999-Exact-Arithmetic-Low]]
- [[Georg-1987-Numerical-Stability-Simplex|research/papers/Georg-1987-Numerical-Stability-Simplex]]
- [[Gill-1974-Methods-Modifying-Matrix|research/papers/Gill-1974-Methods-Modifying-Matrix]]
- [[Gill-1989-Practical-Anti-Cycling|research/papers/Gill-1989-Practical-Anti-Cycling]]
- [[Gomory-1958-Outline-Algorithm-Integer|research/papers/Gomory-1958-Outline-Algorithm-Integer]]
- [[Moler-1967-Rounding-Errors-Algebraic|research/papers/Moler-1967-Rounding-Errors-Algebraic]]
- [[Ogryczak-1987-Numerical-Stability-Simplex|research/papers/Ogryczak-1987-Numerical-Stability-Simplex]]
- [[Unknown-2026-Verified-Linear-Programming|research/papers/Unknown-2026-Verified-Linear-Programming]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic|research/papers/Wilkinson-1963-Rounding-Errors-Algebraic]]
- [[GPU Benefit Unproven|research/research-gaps/GPU Benefit Unproven]]
- [[Deterministic Reduction|research/techniques/Deterministic Reduction]]