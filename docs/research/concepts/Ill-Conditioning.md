---
type: concept
tags: [concepts, numerics]
status: stable
verified_on: 2026-09-25
---

# Ill-Conditioning

> When κ(B) is huge, a "solved" system is only as good as ε·κ — the failure mode SIH asks this solver to survive.

## Definition
The condition number κ(B) = ‖B‖·‖B⁻¹‖ measures how much the solution of Bx = b perturbs under a relative perturbation of B or b; forward error grows like κ(B)·ε_machine. In simplex the critical matrix is the basis B; in interior-point and ADMM paths it is the KKT system or normal equations. Ill-conditioning comes from mismatched row/column magnitudes, near-parallel or weakly redundant rows, and tight bound structures. An LP is "ill-conditioned" when its optimal basis has large κ (Renegar 1994 frames this rigorously).

## Why It Matters Here
- R13 requires handling "ill-conditioned matrices", R9 makes numerical stability the emphasis, R17 the demonstration.
- Observed state: the dual engine aborts when `min|pivot|/max|pivot| < condition_trigger` (default 1e-14, src/lp/dual/dual_simplex.cpp:405-411); `SparseLu` records min/max |pivot| and a growth factor and throws at `singular_tolerance` 1e-14 (src/linalg/sparse_basis.cpp:130-143).
- Inference: there is no κ estimator and no iterative refinement in the loop, so detection exists but recovery does not — accuracy is capped by ε·κ(B).

## Key Facts / Rules
- Rule of thumb: digits of accuracy lost ≈ log10(κ(B)); solvability needs κ(B)·ε ≪ 1.
- O(n²) condition estimation (Cline, Moler, Stewart & Wilkinson 1979) lets a solver monitor κ(B) cheaply per refactorization.
- Scaling reduces κ without changing the mathematics (exact unscale required afterwards).
- Growth factor, not just κ, governs LU forward error (Wilkinson).

## Related
- [[Numerical Stability]]
- [[Scaling]]
- [[Iterative Refinement]]
- [[Degeneracy]]
- [[Cline-1979-Estimate-Condition-Number]]
- [[Renegar-1994-Condition-Numbers-Linear]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Iterative Refinement|research/algorithms/Iterative Refinement]]
- [[Degeneracy|research/concepts/Degeneracy]]
- [[Numerical Stability|research/concepts/Numerical Stability]]
- [[Scaling|research/concepts/Scaling]]
- [[Netlib LP Collection|research/datasets/Netlib LP Collection]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Numerical Error|research/metrics/Numerical Error]]
- [[Bartels-1968-Numerical-Investigation-Simplex|research/papers/Bartels-1968-Numerical-Investigation-Simplex]]
- [[Chinneck-1987-Primal-Dual-Methods|research/papers/Chinneck-1987-Primal-Dual-Methods]]
- [[Chinneck-1992-Feasibility-Redundancy-Linear|research/papers/Chinneck-1992-Feasibility-Redundancy-Linear]]
- [[Cline-1979-Estimate-Condition-Number|research/papers/Cline-1979-Estimate-Condition-Number]]
- [[Gamrath-2015-Progress-Presolving-Mixed|research/papers/Gamrath-2015-Progress-Presolving-Mixed]]
- [[Gartner-1999-Exact-Arithmetic-Low|research/papers/Gartner-1999-Exact-Arithmetic-Low]]
- [[Georg-1987-Numerical-Stability-Simplex|research/papers/Georg-1987-Numerical-Stability-Simplex]]
- [[Gleixner-2015-Iterative-Refinement-Linear|research/papers/Gleixner-2015-Iterative-Refinement-Linear]]
- [[Gondzio-1997-Presolve-Analysis-LPs|research/papers/Gondzio-1997-Presolve-Analysis-LPs]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector|research/papers/Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
- [[Maros-0000-New-Degeneracy-Method|research/papers/Maros-0000-New-Degeneracy-Method]]
- [[Maros-2003-Computational-Optimization-Techniques|research/papers/Maros-2003-Computational-Optimization-Techniques]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior|research/papers/Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Moler-1967-Rounding-Errors-Algebraic|research/papers/Moler-1967-Rounding-Errors-Algebraic]]
- [[OLeary-1981-Equilibrating-Both-Matrices|research/papers/OLeary-1981-Equilibrating-Both-Matrices]]
- [[Ogryczak-1987-Numerical-Stability-Simplex|research/papers/Ogryczak-1987-Numerical-Stability-Simplex]]
- [[Oren-1980-Automatic-Scaling-Matrices|research/papers/Oren-1980-Automatic-Scaling-Matrices]]
- [[Pochet-2006-Production-Planning-Mixed|research/papers/Pochet-2006-Production-Planning-Mixed]]
- [[Ponte-2026-Good-Fast-Row-Sparse|research/papers/Ponte-2026-Good-Fast-Row-Sparse]]
- [[Renegar-1994-Condition-Numbers-Linear|research/papers/Renegar-1994-Condition-Numbers-Linear]]
- [[Steinrucken-2019-Exact-Algorithms-Linear|research/papers/Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Unknown-2026-Verified-Linear-Programming|research/papers/Unknown-2026-Verified-Linear-Programming]]
- [[Vanderbei-1995-Symmetric-Indefinite-Systems|research/papers/Vanderbei-1995-Symmetric-Indefinite-Systems]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic|research/papers/Wilkinson-1963-Rounding-Errors-Algebraic]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]
- [[Zhang-2026-Novel-Linear-Optimization|research/papers/Zhang-2026-Novel-Linear-Optimization]]
- [[Ill-Conditioned Instance Dossier|research/research-gaps/Ill-Conditioned Instance Dossier]]
- [[Ruiz Scaling|research/techniques/Ruiz Scaling]]