---
type: paper
title: "On an Interior Point Algorithm for Linear Programming"
authors: "Ye; Monteiro & Adler"
year: 1991
venue: "(not listed in source)"
doi: "(unverified)"
domain: [ipm]
priority: ○
status: standard
tags: [paper, ipm]
---
# On an Interior Point Algorithm for Linear Programming
> Polynomial iteration bounds for practical path-following variants (Ye 1991; Monteiro & Adler 1989).
## Metadata
| Field | Value |
|---|---|
| Authors | Ye (1991); Monteiro & Adler (1989) |
| Year | 1991 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |
## Problem Addressed
Karmarkar's polynomiality did not automatically carry over to the natural primal-dual iterations practitioners actually wanted to use. The question was whether simple, implementable path-following steps also admit polynomial iteration bounds.
## Core Contribution
- **Methodology:** Projective/path-following primal-dual variants with neighborhood tracking; convergence analysis in the bit/iteration model.
- **Assumptions:** Standard-form LP, strictly feasible or centered start, exact Newton solves within tolerance.
- **Benchmarks/datasets:** None (theory paper).
- **Metrics:** Iteration complexity in input length L and dimension n.
- **Key results:** Polynomial iteration bounds for path-following primal-dual schemes; established the O(sqrt(n) log(1/eps))-style complexity class as the benchmark target (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Primal-dual path-following; projective-scaling variants.
**Techniques:** Neighborhood definitions; short-step rules.
**Implementation details:** Short-step theory prescribes conservative step lengths — impractical codes (Mehrotra) outperform by ignoring the theory while keeping the invariant empirically.
**Limitations/failure cases:** Theoretical step rules are too small for production speed; no sparse-linear-algebra guidance.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Justifies R4 claims of "polynomial, reliable convergence" (R9) when documenting the solver's theoretical basis; no code artifact follows directly from this paper.
## Evidence → Engineering Decision
- *Finding:* Complexity guarantees come from neighborhood invariance, not from any particular implementation → *PS requirement:* R9 → *Component:* src/lp/first_order/pdlp.cpp (PDLP's restart theory plays the analogous role) → *Metric:* [[KKT Residual]] convergence per iteration.
## Related Papers
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
- [[Karmarkar-1984-New-Polynomial-Time-Algorithm]]
## Uses
- [[Interior-Point Method]]
- [[Duality Gap]]
