---
type: research-gap
tags: [research-gaps, numerics]
status: stable
verified_on: 2026-09-25
---

# Ill-Conditioned Instance Dossier

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> R17 asks for a *clear demonstration* of numerical robustness; tests exist, but no curated dossier of hard instances with before/after evidence does.

## Definition
The gap between having robustness mechanisms (tolerances, verifiers, scaling, certificates) and *demonstrating* them: a dossier would be a curated set of ill-conditioned, degenerate and weakly-relaxed instances — with known trouble properties (condition number, degeneracy indicators, root gap), the failure mode naive implementations exhibit, this solver's measured behavior (residuals, iterations, time, status), and reproducible commands. Without such a artifact, "numerically robust" is an assertion; with it, R17 becomes checkable line by line. Safe-bound and certification techniques (Neumaier & Shcherbina) define what a rigorous dossier entry should contain when exact optima are unknown.

## Why It Matters Here
- R17: "A clear demonstration of numerical robustness should be provided" — evaluated as evidence, not as design; audit lists the current state as "degeneracy/ill-conditioning test suites exist, but no curated hard-instance demonstration report" (docs/audit/00-ground-truth.md C.3).
- Observed state: the machinery to *build* the dossier exists — condition/pivot triggers (src/lp/dual/dual_simplex.cpp:405-411), growth-factor diagnostics (src/linalg/sparse_basis.cpp:140-143), independent verifiers, Netlib relative error ≤ 7.9e-15 on 7 instances.
- Inference: the missing work is curation and reporting, plus condition estimation/iterative refinement if instances are found where current accuracy fails.

## Key Facts / Rules
- Dossier entry schema: instance + provenance, difficulty indicators (κ, degeneracy, gap), naive-solver failure mode, our status/residuals/time, command line, hardware.
- Metrics per entry: KKT residual, iterations/refactorizations, condition trigger events, objective vs reference.
- Include at least one case where the solver *declares* failure honestly (condition trigger, iteration limit) — a robustness demo includes known limits.
- Distributional/derived sets (D-MIPLIB, mipfeas) can supply hard instances with reference data.

## Related
- [[Ill-Conditioning]]
- [[Numerical Stability]]
- [[KKT Residual]]
- [[Degeneracy]]
- [[Neumaier-2004-Safe-Bounds-Linear]]
- [[Renegar-1994-Condition-Numbers-Linear]]

## Referenced By

- 16-testing-evaluation-strategy
- 21-traceability
- Research MOC
- [[Iterative Refinement|research/algorithms/Iterative Refinement]]
- cross-paper-synthesis
- [[Numerical Error|research/metrics/Numerical Error]]
- [[Degeneracy Handling Gap|research/research-gaps/Degeneracy Handling Gap]]