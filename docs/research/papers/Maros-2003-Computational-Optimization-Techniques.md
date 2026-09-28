---
type: paper
title: "Computational Optimization Techniques in Linear Programming"
authors: "Maros"
year: 2003
venue: "Book"
doi: "(unverified)"
domain: [survey, lp, numerics]
priority: ○
status: standard
tags: [paper, survey, lp]
---
# Computational Optimization Techniques in Linear Programming

> The reference treatment of numerics, scaling, tolerances and degeneracy inside LP solvers.
## Metadata
| Field | Value |
|---|---|
| Authors | Maros |
| Year | 2003 (approximate — list gives no year for the book) |
| Venue | Book |
| DOI/URL | (unverified) |
## Problem Addressed
LP codes fail on ill-conditioned and degenerate models even when the theory is correct; the field needed a book organized around computational failure modes — factorization, tolerances, scaling, degeneracy — rather than another algorithm derivation.
## Core Contribution
- **Methodology:** Book organizing LP algorithms around numerical behavior: basis factorization, tolerance management, degeneracy, scaling, postoptimal analysis.
- **Assumptions:** Direct-method era; sparse-minded but pre-GPU (approximate).
- **Benchmarks/datasets:** Illustrative LPs (qualitative; no instance list asserted).
- **Metrics:** Condition numbers, residual/forward error, iteration counts (qualitative).
- **Key results:** Practical guidance that tolerance and scaling choices dominate robustness on hard models (qualitative; no figures asserted).
## Engineering-Relevant Knowledge
**Algorithms:** Primal and dual simplex with LU basis factorization and refactorization.
**Techniques:** Row/column scaling, tolerance selection, degeneracy/anti-cycling handling, reuse of factorizations.
**Implementation details:** Directly motivates our tolerance constants, refactorization triggers and Scaling passes; the book is the justification for treating numerics as a first-class module, not an afterthought.
**Equations/rules:** Condition number κ(A) bounds attainable accuracy in the basis solve; residual checks catch factorization drift (Numerical Error).
**Limitations/failure cases:** Predates modern parallel/GPU solver architecture; some specific algorithms superseded (approximate).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the numerics playbook for R9/R13: every robustness feature we must demonstrate (scaling, tolerances, degeneracy handling, refactorization policy) is argued here with implementation-level reasoning.
## Evidence → Engineering Decision
- *Finding:* Tolerance and scaling choices determine robustness on degenerate/ill-conditioned models → *PS requirement:* R9 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Numerical Error
## Related Papers
- [[Bixby-2002-Evolution-of-LP]] [[Oren-1980-Automatic-Scaling-Matrices]] [[Curtis-1972-Simplex-LU-Decomposition]]
## Uses
- [[Ill-Conditioning]] [[Scaling]] [[Numerical Stability]]
