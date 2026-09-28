---
type: paper
title: "Analysis of Mathematical Programming Problems Prior to Applying the Simplex Method"
authors: "Brearley, Mitra & Williams"
year: 1975
venue: "JOTA"
doi: "(unverified)"
domain: [presolve]
priority: ○
status: standard
tags: [paper, presolve]
---
# Analysis of Mathematical Programming Problems Prior to Applying the Simplex Method
> The original presolve paper: bound propagation, redundancy and implied bounds before the first pivot.
## Metadata
| Field | Value |
|---|---|
| Authors | Brearley, Mitra & Williams |
| Year | 1975 |
| Venue | JOTA |
| DOI/URL | (unverified) |
## Problem Addressed
Models often carry obvious redundancy — empty rows, fixed variables, bounded-out rows — that wastes simplex work and destabilizes the basis. The authors formalized what can be checked cheaply before any simplex iteration.
## Core Contribution
- **Methodology:** Pre-simplex analysis: detect empty/fixed structure, propagate bounds, identify redundant or infeasible rows cheaply, reduce the model.
- **Assumptions:** LP in general form; exact-enough arithmetic for bound comparisons.
- **Benchmarks/datasets:** Hand-analyzed models of the era (qualitative).
- **Metrics:** Model size reduction; avoidance of degenerate pivots.
- **Key results:** Demonstrated that trivial analysis removes meaningful structure before solve (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Pre-simplex model analysis.
**Techniques:** Bound propagation; redundancy tests.
**Implementation details:** The direct ancestor of our 4 implemented rules (empty row, empty column, row singleton, fixed variable) in `src/presolve/presolve.cpp`.
**Limitations/failure cases:** Limited to LP-safe inferences; no MIP logic, no dual recovery discussion (later supplied by Andersen).
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 baseline. Good as the minimal correctness reference: our current presolve should match all Brearley-safe rules before adding MIP ones.
## Evidence → Engineering Decision
- *Finding:* Trivial pre-solve analysis pays for itself with near-zero risk → *PS requirement:* R5 → *Component:* src/presolve/presolve.cpp → *Metric:* presolve on/off solve-time delta (CLI `--no-presolve`).
## Related Papers
- [[Andersen-1995-Presolving-Linear-Programming]]
- [[Mahajan-2011-Presolving-Mixed-Integer-Linear]]
## Uses
- [[Presolve]]
- [[Reduced Cost]]
