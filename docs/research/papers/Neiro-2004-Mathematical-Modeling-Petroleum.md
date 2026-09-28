---
type: paper
title: "Mathematical Modeling of Petroleum Refinery (incl. Samsioe linear blending LP)"
authors: "Neiro, Ramos & Mendes"
year: 2004
venue: "Computers & Chemical Engineering"
doi: "(unverified)"
domain: [domain]
priority: ○
status: standard
tags: [paper, domain]
---
# Mathematical Modeling of Petroleum Refinery (incl. Samsioe linear blending LP)
> Refinery scheduling/crude-blending formulations — the MRPL-facing model family the PS names first.
## Metadata
| Field | Value |
|---|---|
| Authors | Neiro, Ramos & Mendes (+ Samsioe linear blending LP, per entry) |
| Year | 2004 (no ★/✦/○ marker on this entry in source list; classified ○) |
| Venue | Computers & Chemical Engineering |
| DOI/URL | (unverified) — Errata note: domain-application entries still need DOI lookups |

## Problem Addressed
A refinery must select crudes, blend components into products meeting quality specs (octane, RVP, sulphur, density), and schedule tanks/units under capacity and demand limits. The paper addresses the mathematical formulation of this decision problem — LP for blending economics, extended models for scheduling.
## Core Contribution
- **Methodology:** LP/MINLP refinery models: blending quality indices combined linearly (blending weights), crude selection and product demand constraints, scheduling of unit charges over a horizon.
- **Assumptions:** Quality properties blend approximately linearly (index-based); deterministic demands/prices for the LP layer.
- **Benchmarks/datasets:** Refinery case data from open literature (not re-verified).
- **Metrics:** Profit/margin objective, constraint satisfaction on product specs, model size and solve time.
- **Key results:** Linear blending LPs capture most blending economics and solve fast; nonconvex quality blending drives MINLP extensions (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Blending LP; refinery scheduling MILP/MINLP formulations.
**Techniques:** Quality-index linearization (weighted blending), product pooling, tank/time-indexed scheduling variables.
**Implementation details:** Our demo instances `blend.mps` and `refinery-feasible.mps` are 2×2 toys — R11/R12 expect industrial-scale sparse models; canonicalization (`src/transform/canonicalize.cpp`) must handle blending matrices with wide dense-ish rows.
**Limitations/failure cases:** Linear blending approximations misprice nonlinear quality behavior (e.g. octane blending) — known source of infeasible/ill-conditioned industrial LPs.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It defines the primary use case named by MRPL (R11) and tells us what model structures (blending, pooling, scheduling) our MPS pipeline must handle credibly.
## Evidence → Engineering Decision
- *Finding:* Refinery demo instances in-repo are 2×2 toy models (evidence/benchmarks/phase4.json descriptions) → *PS requirement:* R12 → *Component:* src/transform/canonicalize.cpp → *Metric:* rows/cols/NNZ of the largest refinery-class instance solved
## Related Papers
- [[Kallrath-2002-Planning-Scheduling-Industry]]
- [[Pochet-2006-Production-Planning-Mixed]]
## Uses
- [[Sparsity]]
