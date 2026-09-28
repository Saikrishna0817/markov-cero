---
type: concept
tags: [algorithms, numerics]
status: stable
verified_on: 2026-09-25
---

# Iterative Refinement

> Solve, measure the residual in higher precision, solve the correction — recover digits a single-precision factorization cannot hold.

## Definition
Iterative refinement factorizes the matrix once in working precision, then repeatedly computes the residual r = b − Ax (ideally in higher precision), solves for a correction, and accumulates it until the residual stops improving. It is backward stable in the *computed* factorization and yields forward accuracy whenever κ(B)·ε_working < 1, so it converts a modest-precision factorization into a high-accuracy solve at the cost of a few extra triangular sweeps. For LP, refinement can be pushed beyond residuals to iterates — SoPlex-style exact/fixed-dimension refinement drives solutions to certified accuracy when floating point alone stalls.

## Why It Matters Here
- R9/R17: this is the standard, cheap recovery mechanism for the accuracy lost to ill-conditioning — and it is absent from the repo.
- Observed state: no refinement loop exists in the simplex or KKT paths (search of src/lp, src/linalg, src/qp shows only single-accuracy solves); accuracy claims rest on post-hoc verification against tolerances instead.
- Inference: [[First-Order Accuracy Ceiling]] and part of the [[Ill-Conditioned Instance Dossier]] gap are attributable to the absence of refinement plus a condition estimator.

## Key Facts / Rules
- Converges to full working-precision accuracy iff κ(B)·ε < 1; otherwise it cannot create information.
- Residual must be computed in higher precision, otherwise refinement buys nothing (Moler 1967).
- One factorization + k solves is much cheaper than k factorizations — hence the cost-benefit.
- Applied to LP: refine primal/dual iterates until KKT residual is certified at a tighter tolerance.

## Related
- [[Ill-Conditioning]]
- [[Numerical Stability]]
- [[KKT Residual]]
- [[Numerical Error]]
- [[Gleixner-2015-Iterative-Refinement-Linear]]
- [[Moler-1967-Rounding-Errors-Algebraic]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[21-traceability|audit/21-traceability]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Ill-Conditioning|research/concepts/Ill-Conditioning]]
- [[Numerical Stability|research/concepts/Numerical Stability]]
- [[First-Order Accuracy Ceiling|research/limitations/First-Order Accuracy Ceiling]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[KKT Residual|research/metrics/KKT Residual]]
- [[Numerical Error|research/metrics/Numerical Error]]
- [[Azulay-0000-Revised-Simplex-Method|research/papers/Azulay-0000-Revised-Simplex-Method]]
- [[Bartels-1969-Simplex-LU-Decomposition|research/papers/Bartels-1969-Simplex-LU-Decomposition]]
- [[Bartels-1971-Stabilization-Simplex|research/papers/Bartels-1971-Stabilization-Simplex]]
- [[Cline-1979-Estimate-Condition-Number|research/papers/Cline-1979-Estimate-Condition-Number]]
- [[Gartner-1999-Exact-Arithmetic-Low|research/papers/Gartner-1999-Exact-Arithmetic-Low]]
- [[Gill-0000-Two-Phase-Algorithms|research/papers/Gill-0000-Two-Phase-Algorithms]]
- [[Gill-1974-Methods-Modifying-Matrix|research/papers/Gill-1974-Methods-Modifying-Matrix]]
- [[Gleixner-2015-Iterative-Refinement-Linear|research/papers/Gleixner-2015-Iterative-Refinement-Linear]]
- [[Grigori-2007-Parallel-Symbolic-Factorization|research/papers/Grigori-2007-Parallel-Symbolic-Factorization]]
- [[Lawson-1974-Solving-Least-Squares|research/papers/Lawson-1974-Solving-Least-Squares]]
- [[Moler-1967-Rounding-Errors-Algebraic|research/papers/Moler-1967-Rounding-Errors-Algebraic]]
- [[Ogryczak-1987-Numerical-Stability-Simplex|research/papers/Ogryczak-1987-Numerical-Stability-Simplex]]
- [[Unknown-2026-Verified-Linear-Programming|research/papers/Unknown-2026-Verified-Linear-Programming]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic|research/papers/Wilkinson-1963-Rounding-Errors-Algebraic]]