---
type: paper
title: "Safe Bounds in Linear and Mixed-Integer Linear Programming"
authors: "Neumaier & Shcherbina"
year: 2004
venue: "Mathematical Programming"
doi: "(unverified)"
domain: [benchmark, numerics]
priority: ○
status: standard
tags: [paper, benchmark]
---
# Safe Bounds in Linear and Mixed-Integer Linear Programming
> Rigorous (outward-rounded) lower/upper bounds for LP and MILP — certification that survives floating-point failure and ill-posed data; shared note for Module 11 #153 and Module 13 #192.
## Metadata
| Field | Value |
|---|---|
| Authors | Neumaier & Shcherbina |
| Year | 2004 |
| Venue | Mathematical Programming |
| DOI/URL | (unverified); listed twice in source list (M11 #153, M13 cross-ref #192) |

## Problem Addressed
On ill-conditioned or uncertainty-laden models a computed "bound" may be neither a valid lower nor a valid upper bound, so any optimality claim is unsound. The paper asks how to produce bounds guaranteed valid despite directed rounding and uncertain coefficients — including for benchmark instances whose reference values themselves are doubtful.
## Core Contribution
- **Methodology:** Interval/verified computation with outward rounding to derive rigorous lower and upper bounds for LP/MILP; certified dual bounds; safe handling of infeasibility and data error.
- **Assumptions:** IEEE directed rounding available; formulation bounded; error model or interval data declared.
- **Benchmarks/datasets:** Ill-posed/uncertain LP and MILP examples; used to certify optima on problematic benchmark instances (per M13 #192).
- **Metrics:** Certified interval width (rigorous gap), zero probability of false certification, cost overhead vs. plain solve.
- **Key results:** Safe bounds are computable at practical cost in many cases, converting "we believe it is optimal" into a certificate (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Verified LP bound computation; MILP bounding under interval data.
**Techniques:** Outward rounding; fast ordinary bounds on a fast path with safe fallback only for the final certificate.
**Implementation details:** Complements `src/verify/primal_verifier.cpp`: we verify feasibility in floating point but certify no rigorous bound — BLEND is the concrete failure (our simplex errors out while PDLP converges); compute safe bounds only at the reporting boundary, not per node.
**Limitations/failure cases:** Interval arithmetic costs roughly 2–10× (per companion note) and widens on ill-conditioned problems; over-conservative bounds can mask true optimality.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R17/R9 demand demonstrable numerical robustness; a rigorous bound is the strongest form of "solved where simpler implementations fail" — and #192 makes it a benchmarking requirement for ill-posed instances.
## Evidence → Engineering Decision
- *Finding:* BLEND → NumericalFailure for our simplex while PDLP reaches −30.8120352 (_deployment-phase2-build/gpu_benchmark.csv) → *PS requirement:* R17 → *Component:* src/verify/primal_verifier.cpp → *Metric:* certified objective interval vs. reported optimum
## Related Papers
- [[Anderssen-1984-NETLIB-LP-Test-Set]]
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
## Uses
- [[Numerical Stability]]
