---
type: paper
title: "Presolve Analysis of LPs Prior to Applying an Interior Point Method"
authors: "Gondzio"
year: 1997
venue: "IJOC"
doi: "(unverified)"
domain: [ipm, presolve]
priority: ○
status: standard
tags: [paper, presolve]
---
# Presolve Analysis of LPs Prior to Applying an Interior Point Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Presolve engineered for IPM: dense-column splitting, implied free variables, dependent-row handling before the first Newton solve.
## Metadata
| Field | Value |
|---|---|
| Authors | Gondzio |
| Year | 1997 |
| Venue | IJOC |
| DOI/URL | (unverified) |
## Problem Addressed
Simplex presolve leaves structure that hurts interior-point methods specifically: dense columns inflate the Schur complement, explicit free variables inflate the system, linearly dependent rows make the KKT system singular. IPM needs its own presolve pass.
## Core Contribution
- **Methodology:** Pre-IPM reductions: split dense columns into linked copies, detect implied-free variables from bounds, remove/regularize dependent rows, exploit implied bounds to tighten.
- **Assumptions:** LP in standard/bounded form; tolerances for bound inference; reversible transformations recorded for postsolve.
- **Benchmarks/datasets:** Netlib LPs with known dense-column problems.
- **Metrics:** Reductions in rows/cols/nonzeros; factorization time; iterations.
- **Key results:** Measurable factorization/iteration savings on LPs with dense columns and dependent rows (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** IPM-specific presolve; dense-column decomposition.
**Techniques:** Implied-free detection; dependent-row removal; column splitting (one column -> two linked columns).
**Implementation details:** Our presolve (`src/presolve/presolve.cpp`) implements empty rows, empty columns, row singletons and fixed variables (see `PresolveStatistics` in `include/markov_cero/presolve/presolve.hpp`) — none of Gondzio's IPM-specific rules are present.
**Limitations/failure cases:** Splitting increases row count; dependent-row detection needs rank-revealing tolerance choices (ties to [[Ill-Conditioning]]).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 names presolve, R6 sparse linear algebra; these rules make a future IPM (R4) and even PDLP faster on industrial models with dense rows/columns (R11 refinery models often have them).
## Evidence → Engineering Decision
- *Finding:* Dense columns dominate Schur-complement cost → *PS requirement:* R5, R6 → *Component:* src/presolve/presolve.cpp (add dense-column split + implied-free rules) → *Metric:* presolved nnz count, IPM/PDLP factorization time share.
- *Finding:* Dependent rows make Newton systems singular → *PS requirement:* R13 → *Component:* src/presolve/presolve.cpp → *Metric:* solve failure rate (exit 7).
## Related Papers
- [[Andersen-1995-Presolving-Linear-Programming]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
## Uses
- [[Presolve]]
