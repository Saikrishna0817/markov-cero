---
type: paper
title: "Linear Programming: Foundations and Extensions"
authors: "Vanderbei"
year: 1996
venue: "Book"
doi: "(unverified)"
domain: [survey, lp]
priority: ○
status: standard
tags: [paper, survey, lp]
---
# Linear Programming: Foundations and Extensions

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Standard LP text deriving simplex and interior-point methods from duality with an implementer's eye.
## Metadata
| Field | Value |
|---|---|
| Authors | Vanderbei |
| Year | 1996 (approximate — first edition; list gives no year) |
| Venue | Book |
| DOI/URL | (unverified) |
## Problem Addressed
Practitioners need a single source that derives both the simplex method and barrier methods from the same duality/KKT machinery, with worked numerical examples and the extensions (networks, integer programming) that appear in real models.
## Core Contribution
- **Methodology:** Textbook: model formulation, duality theory, revised simplex, interior-point methods, network LP and integer extensions.
- **Assumptions:** Undergraduate-level mathematics; edition/year approximate.
- **Benchmarks/datasets:** Small worked examples (Netlib-style exercises, qualitative).
- **Metrics:** Not applicable (pedagogical).
- **Key results:** Complete derivations usable as an implementation spec for the revised simplex loop and KKT-based optimality checks (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex, primal-dual interior point, network simplex.
**Techniques:** Duality-based infeasibility/unboundedness certificates, tableau vs. revised forms.
**Implementation details:** Worked examples become unit-test oracles for src/lp/reference/revised_simplex.cpp (basis updates, pricing, ratio test) without importing any external solver code.
**Equations/rules:** Primal/dual pair forms, reduced-cost optimality test, KKT Conditions as the stopping rule shared by simplex and IPM.
**Limitations/failure cases:** Pedagogical scale; no sparse large-scale or degeneracy engineering (see Maros for that).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Gives R10-compliant, from-mathematics specifications and hand-checked test vectors for R4's revised simplex; its KKT statements also feed R17's robustness checks.
## Evidence → Engineering Decision
- *Finding:* Textbook worked examples can serve as independent correctness oracles → *PS requirement:* R10 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* Numerical Error
## Related Papers
- [[Dantzig-1963-Linear-Programming-Extensions]] [[Maros-2003-Computational-Optimization-Techniques]] [[Bixby-2002-Evolution-of-LP]]
## Uses
- [[KKT Conditions]] [[Reduced Cost]]
