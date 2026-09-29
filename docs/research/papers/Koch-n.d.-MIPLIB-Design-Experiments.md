---
type: paper
title: "MIPLIB 2017 — Design and Experiments + Solution Checker"
authors: "Koch et al."
year: n.d.
venue: "(unverified)"
doi: "(unverified)"
domain: [benchmark]
priority: ○
status: standard
tags: [paper, benchmark]
---
# MIPLIB 2017 — Design and Experiments + Solution Checker

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> The engineering companion to MIPLIB 2017: how the library was built and tested, plus the solution checker that independently certifies feasibility and optimality of returned solutions.
## Metadata
| Field | Value |
|---|---|
| Authors | Koch et al. (as listed; full author list not given) |
| Year | n.d. (no year stated in source list) |
| Venue | (unverified) |
| DOI/URL | (unverified) |

## Problem Addressed
A benchmark library is only usable if returned solutions can be checked independently: a solver may claim optimality on an infeasible point or report a wrong objective. The entry covers the design/experiment record of MIPLIB 2017 and the checker used to validate submissions.
## Core Contribution
- **Methodology:** Compilation pipeline for the library (collection, verification, documentation) plus an external solution checker validating feasibility, integrality and objective value against stored reference data.
- **Assumptions:** Solutions are submitted in a standard format; tolerances for feasibility/integrality are published.
- **Benchmarks/datasets:** MIPLIB 2017 collection and benchmark set.
- **Metrics:** Checker verdicts: max constraint violation, integrality violation, objective deviation, gap vs. best-known.
- **Key results:** Independent checking is part of the library definition, not an optional extra (details not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Solution verification (primal feasibility, integrality, objective comparison).
**Techniques:** Zero-trust checking: never accept a solver's own certificate; recompute residuals from the raw instance.
**Implementation details:** We already have `src/verify/primal_verifier.cpp` and a zero-trust verifier claim; the missing piece is storing MIPLIB best-known objectives/acceptance tolerances and running the official checker on our outputs.
**Limitations/failure cases:** Checker without a reference-solution database cannot certify *optimality*, only feasibility.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R15/R16 need verified MIPLIB results; adopting the official checker is the cheapest way to make our MIPLIB claims externally credible.
## Evidence → Engineering Decision
- *Finding:* Our verification is in-tree self-verification with 3 MIPLIB instances (evidence/miplib_results.csv) → *PS requirement:* R15 → *Component:* src/verify/primal_verifier.cpp → *Metric:* agreement with official MIPLIB solution checker
## Related Papers
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Achterberg-2005-MIPLIB-2003]]
## Uses
- [[MIPLIB]]
