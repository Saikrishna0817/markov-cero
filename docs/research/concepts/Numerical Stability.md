---
type: concept
tags: [concepts, numerics]
status: stable
verified_on: 2026-09-25
---

# Numerical Stability

> Backward stability is the property that lets you believe an answer at all; without it every downstream number is decoration.

## Definition
An algorithm is backward stable if the computed result is the exact solution of the problem with slightly perturbed data (‖ΔA‖ ≲ ε‖A‖, componentwise where possible). For linear algebra the levers are pivot choice, growth factor, refactorization policy and tolerances; for simplex additionally the *acceptance tolerances* of ratio tests, because near-ties in floating point decide which vertex you walk to. Stability is not implied by a correct algorithm — the same Bartels–Golub update is stable or not depending on how errors are monitored (Georg & Hettich; Ogryczak 1987). Forward accuracy additionally requires κ(B)·ε ≪ 1, which no pivot rule can fix.

## Why It Matters Here
- R9/R17 make stability the emphasis and require a demonstration; R13 ties it to degeneracy and ill-conditioning.
- Observed state: the safety net exists in pieces — singular tolerance 1e-14, growth-factor diagnostics (src/linalg/sparse_basis.cpp:140-143), Harris ratio in the dual engine, a pivot-ratio condition trigger, and post-hoc verification of every terminal reference result.
- Inference: there is no iterative refinement and no condition estimator, so detection is present but recovery (extra accurate solves) is not.

## Key Facts / Rules
- Backward error criterion: computed x solves (A+ΔA)x = b with ‖ΔA‖ bounded by ε·growth terms.
- Digits lost ≈ log10 κ(B); iterative refinement recovers them only while κ(B)·ε < 1.
- Tolerances must be *error-estimating* (scale-relative), not fixed absolute constants — otherwise scaling silently changes behavior.
- Certificates (Farkas rays, KKT residuals) are the checkable form of "the answer is trustworthy".

## Related
- [[Ill-Conditioning]]
- [[Scaling]]
- [[Iterative Refinement]]
- [[Harris Ratio Test]]
- [[Numerical Error]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Iterative Refinement|research/algorithms/Iterative Refinement]]
- [[Markowitz Pivoting|research/algorithms/Markowitz Pivoting]]
- [[Ill-Conditioning|research/concepts/Ill-Conditioning]]
- [[Scaling|research/concepts/Scaling]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Numerical Error|research/metrics/Numerical Error]]
- [[Andersen-1995-Presolving-Linear-Programming|research/papers/Andersen-1995-Presolving-Linear-Programming]]
- [[Azulay-0000-Revised-Simplex-Method|research/papers/Azulay-0000-Revised-Simplex-Method]]
- [[Bartels-1968-Numerical-Investigation-Simplex|research/papers/Bartels-1968-Numerical-Investigation-Simplex]]
- [[Bartels-1971-Stabilization-Simplex|research/papers/Bartels-1971-Stabilization-Simplex]]
- [[Berthold-2023-Feasibility-Jump|research/papers/Berthold-2023-Feasibility-Jump]]
- [[Charnes-1954-Optimality-Multi-Valuedness|research/papers/Charnes-1954-Optimality-Multi-Valuedness]]
- [[Cline-1979-Estimate-Condition-Number|research/papers/Cline-1979-Estimate-Condition-Number]]
- [[Curtis-1972-Simplex-LU-Decomposition|research/papers/Curtis-1972-Simplex-LU-Decomposition]]
- [[Dantzig-1954-Product-Form-Inverse|research/papers/Dantzig-1954-Product-Form-Inverse]]
- [[Forrest-1972-Updating-Triangular-Factors|research/papers/Forrest-1972-Updating-Triangular-Factors]]
- [[Fourer-n.d.-Solving-Dense-Linear|research/papers/Fourer-n.d.-Solving-Dense-Linear]]
- [[Gartner-1999-Exact-Arithmetic-Low|research/papers/Gartner-1999-Exact-Arithmetic-Low]]
- [[Georg-1987-Numerical-Stability-Simplex|research/papers/Georg-1987-Numerical-Stability-Simplex]]
- [[Gill-0000-Two-Phase-Algorithms|research/papers/Gill-0000-Two-Phase-Algorithms]]
- [[Gill-1974-Methods-Modifying-Matrix|research/papers/Gill-1974-Methods-Modifying-Matrix]]
- [[Gleixner-2012-Factorization-Update-Reduced|research/papers/Gleixner-2012-Factorization-Update-Reduced]]
- [[Gleixner-2015-Iterative-Refinement-Linear|research/papers/Gleixner-2015-Iterative-Refinement-Linear]]
- [[Harris-1973-Pivot-Selection-Methods|research/papers/Harris-1973-Pivot-Selection-Methods]]
- [[Lawson-1974-Solving-Least-Squares|research/papers/Lawson-1974-Solving-Least-Squares]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector|research/papers/Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Maros-2003-Computational-Optimization-Techniques|research/papers/Maros-2003-Computational-Optimization-Techniques]]
- [[Mehlhorn-2010-Implementation-Simplex-Algorithm|research/papers/Mehlhorn-2010-Implementation-Simplex-Algorithm]]
- [[Moler-1967-Rounding-Errors-Algebraic|research/papers/Moler-1967-Rounding-Errors-Algebraic]]
- [[Neumaier-2004-Safe-Bounds-Linear|research/papers/Neumaier-2004-Safe-Bounds-Linear]]
- [[Ogryczak-1987-Numerical-Stability-Simplex|research/papers/Ogryczak-1987-Numerical-Stability-Simplex]]
- [[Steinrucken-2019-Exact-Algorithms-Linear|research/papers/Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Suhl-1990-Fast-LU-Factorization|research/papers/Suhl-1990-Fast-LU-Factorization]]
- [[Unknown-2026-Verified-Linear-Programming|research/papers/Unknown-2026-Verified-Linear-Programming]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic|research/papers/Wilkinson-1963-Rounding-Errors-Algebraic]]
- [[Zhang-2025-Combined-Linear-Nonlinear|research/papers/Zhang-2025-Combined-Linear-Nonlinear]]
- [[Ill-Conditioned Instance Dossier|research/research-gaps/Ill-Conditioned Instance Dossier]]
- [[Harris Ratio Test|research/techniques/Harris Ratio Test]]