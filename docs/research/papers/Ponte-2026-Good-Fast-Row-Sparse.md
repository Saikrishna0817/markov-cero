---
type: paper
title: "Good and Fast Row-Sparse AH-Symmetric Reflexive Generalized Inverses"
authors: "Ponte, Fampa, Lee & Xu"
year: 2026
venue: "(unverified)"
doi: "(unverified)"
domain: [sparse]
priority: ✦
status: standard
tags: [paper, sparse]
---

# Good and Fast Row-Sparse AH-Symmetric Reflexive Generalized Inverses
> Constructs sparse generalized inverses quickly — adjacent to how singular systems and bases are handled.
## Metadata
| Field | Value |
|---|---|
| Authors | Ponte, Fampa, Lee & Xu |
| Year | 2026 (as listed) |
| Venue | (unverified) |
| DOI/URL | https://arxiv.org/abs/2401.17540 |
## Problem Addressed
Generalized inverses of structured (AH-symmetric, reflexive) matrices are needed in estimation and rank-deficient least-squares work, but general constructions are slow and destroy sparsity; the paper gives methods producing row-sparse generalized inverses quickly.
## Core Contribution
- **Methodology:** Algebraic construction exploiting AH-symmetry and reflexivity, with algorithms that preserve row-sparsity of the resulting inverse (approximate).
- **Assumptions:** Structured matrix classes; rank-deficiency allowed; sparsity of the inverse is the objective (approximate).
- **Benchmarks/datasets:** Structured test matrices (qualitative; no figures asserted).
- **Metrics:** Construction time, row-sparsity of output, approximation quality (qualitative).
- **Key results:** Fast construction of row-sparse reflexive generalized inverses for the structured classes (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Generalized-inverse construction for rank-deficient systems; alternative to full pseudoinverse computation.
**Techniques:** Structure-exploiting algebra, sparsity-preserving rank-deficient solves.
**Implementation details:** Marginally relevant: LP bases are kept nonsingular by pivoting, so we use LU solves, not generalized inverses; the transferable idea is preferring structure-exploiting sparse construction over dense fallbacks when rank deficiency appears.
**Equations/rules:** A is a reflexive generalized inverse if AXA = A and XAX = X; row-sparsity bounds storage/solve cost.
**Limitations/failure cases:** Generalized inverses are the wrong tool for LP basis solves (stability and cost); niche structure (AH-symmetric) rarely occurs in raw LP matrices.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Rank deficiency shows up in dependent-row models (R13); this shows how to stay sparse under rank deficiency, but our presolve/scaling route (drop dependent rows) is the simpler, more robust answer for a solver core.
## Evidence → Engineering Decision
- *Finding:* Under rank deficiency, structure-exploiting sparse methods beat dense pseudoinverses → *PS requirement:* R13 → *Component:* src/presolve/presolve.cpp → *Metric:* Numerical Error
## Related Papers
- [[OLeary-1981-Equilibrating-Both-Matrices]] [[Bartels-1969-Simplex-LU-Decomposition]] [[Howell-2018-Prestructuring-Sparse-Matrices]]
## Uses
- [[Sparsity]] [[Ill-Conditioning]]
