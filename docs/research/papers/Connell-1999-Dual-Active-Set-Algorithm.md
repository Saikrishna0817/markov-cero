---
type: paper
title: "A Dual Active-Set Algorithm for Positive Semidefinite Quadratic Programming"
authors: "Connell"
year: 1999
venue: "Math. Prog."
doi: "(unverified)"
domain: [qp]
priority: ○
status: standard
tags: [paper, qp]
---
# A Dual Active-Set Algorithm for Positive Semidefinite Quadratic Programming
> Extends Goldfarb-Idnani to PSD (not strictly PD) Q — the case our convexity check must tolerate.
## Metadata
| Field | Value |
|---|---|
| Authors | Connell |
| Year | 1999 |
| Venue | Math. Prog. |
| DOI/URL | (unverified) |
## Problem Addressed
Goldfarb-Idnani requires strictly positive definite Q, so semidefinite objectives (many variables unpenalized, redundant quadratic terms) are excluded. Industrial QPs frequently have PSD-but-not-PD Hessians, especially after variable aggregation.
## Core Contribution
- **Methodology:** Dual active-set method adapted for PSD Q: dual objective no longer strictly convex, so step directions are chosen on the subspace where Q is positive; handles non-unique primal optima by selecting a particular solution.
- **Assumptions:** Q positive semidefinite; linear constraints; rank identification on the support of Q.
- **Benchmarks/datasets:** PSD QP test problems (paper's set).
- **Metrics:** Optimality residuals; active-set iterations.
- **Key results:** Correct handling of PSD instances where GI fails (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Dual active-set QP for PSD objectives.
**Techniques:** Null-space handling; subspace step selection.
**Implementation details:** Our convexity verification in `src/qp/model.cpp` does an LDL^T diagonal pivot check — semidefinite cases are near the threshold and currently risky for any active-set route; ADMM tolerates PSD more gracefully (needs only convexity, not strict).
**Limitations/failure cases:** Non-uniqueness complicates termination; degenerate active sets cause cycling without anti-cycling rules ([[Degeneracy]]).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R2 (QP) with realistic industrial Hessians; guides how our solver should behave when Q is PSD at the verification boundary.
## Evidence → Engineering Decision
- *Finding:* PSD Q breaks strictly-convex active-set assumptions but is fine for ADMM → *PS requirement:* R2, R13 → *Component:* src/qp/model.cpp (convexity classification) → *Metric:* correct accept/reject of PSD models; verifier pass rate.
## Related Papers
- [[Goldfarb-1983-Numerically-Stable-Dual]]
- [[Goldfarb-1984-Dual-Primal-Dual-Methods]]
## Uses
- [[KKT Conditions]]
