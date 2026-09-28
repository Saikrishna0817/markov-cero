---
type: paper
title: "Interior Point Methods for LP: Computational State of the Art"
authors: "Lustig, Marsten & Shanno"
year: 1994
venue: "IJOC"
doi: "10.1287/ijoc.6.1.1 (derived from list link)"
domain: [ipm]
priority: ○
status: standard
tags: [paper, ipm]
---
# Interior Point Methods for LP: Computational State of the Art
> Survey + OB1 implementation report: presolve, ordering and factorization choices that decide whether an IPM is fast.
## Metadata
| Field | Value |
|---|---|
| Authors | Lustig, Marsten & Shanno |
| Year | 1994 |
| Venue | IJOC |
| DOI/URL | 10.1287/ijoc.6.1.1 |
## Problem Addressed
By 1994 many IPM variants existed but comparisons were inconsistent and implementation details (presolve, linear algebra ordering, refactorization policy) were under-reported. The paper documents the OB1 code and frames what "state of the art" meant computationally.
## Core Contribution
- **Methodology:** Comparative account of primal-dual codes; OB1 specifics: presolve, column ordering, factorization and accuracy control.
- **Assumptions:** Sparse standard-form LPs; a working sparse LDL^T/Cholesky kernel; comparable tolerance settings across codes.
- **Benchmarks/datasets:** Netlib LP collection.
- **Metrics:** Wall time, iterations, robustness across the test set.
- **Key results:** OB1 competitive with the best contemporaneous codes on Netlib (qualitative; per-instance numbers in paper).
## Engineering-Relevant Knowledge
**Algorithms:** Primal-dual IPM (Mehrotra family) with presolve.
**Techniques:** Fill-reducing ordering before factorization; presolve to shrink the Newton system.
**Implementation details:** Directly parallels markov-cero gaps: presolve exists (`src/presolve/presolve.cpp`, 4 rules) but no IPM module; ordering infra is partial ([[Symbolic Factorization]] not implemented).
**Limitations/failure cases:** 1994-era conclusions predate GPU/first-order methods; dense-column pathologies still apply.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R4 (IPM) + R6 (sparse linear algebra). Use as a checklist when designing `src/lp/ipm/`: presolve before IPM, ordering before factorization, uniform tolerances.
## Evidence → Engineering Decision
- *Finding:* IPM speed depends as much on ordering/presolve as on the algorithm → *PS requirement:* R4, R6 → *Component:* src/presolve/presolve.cpp + proposed ordering in src/linalg/ → *Metric:* factorization time share, geometric mean runtime.
## Related Papers
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]
## Uses
- [[Interior-Point Method]]
