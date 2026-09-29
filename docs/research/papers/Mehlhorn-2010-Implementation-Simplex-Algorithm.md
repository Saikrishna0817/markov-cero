---
type: paper
title: "Implementation of the Simplex Algorithm"
authors: "Mehlhorn"
year: 2010
venue: "(unverified)"
doi: "(unverified)"
domain: [lp]
priority: ✦
status: standard
tags: [paper, lp]
---

# Implementation of the Simplex Algorithm

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Practical revised-simplex implementation guide: round-off, sparsity and a public-domain code comparison.
## Metadata
| Field | Value |
|---|---|
| Authors | Mehlhorn |
| Year | 2010 |
| Venue | (unverified) |
| DOI/URL | https://resources.mpi-inf.mpg.de/departments/d1/teaching/ss10/Obst/Simplex.pdf |
## Problem Addressed
Textbooks present the simplex abstractly while production codes differ in dozens of engineering choices (data layout, pricing loops, tolerance handling, refactorization); implementers need one source that walks through an actual code and explains why each choice is made.
## Core Contribution
- **Methodology:** Walkthrough of a revised-simplex implementation with discussion of round-off behavior, sparsity exploitation and comparisons against public-domain codes (approximate).
- **Assumptions:** Reader implements from scratch; numerical details matter as much as the algorithm (approximate).
- **Benchmarks/datasets:** Small/medium LPs used for code comparison (qualitative; no figures asserted).
- **Metrics:** Runtime and solution quality across implementation variants (qualitative).
- **Key results:** Concrete implementation guidance and evidence that engineering details dominate naive implementations (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Revised simplex end-to-end: model form, basis, pricing, ratio test, factorization, termination.
**Techniques:** Round-off management, sparse data structures, code-level comparison methodology.
**Implementation details:** Closest in spirit to our src/lp/reference/revised_simplex.cpp — useful as a checklist that our reference implementation is complete (I/O, tolerances, degeneracy exits, verification) rather than a bare pivot loop.
**Equations/rules:** Standard optimality/feasibility tests restated with attention to how floating point changes them (KKT Conditions).
**Limitations/failure cases:** Lecture-note form; no rigorous error analysis (see Bartels 1971) and no modern large-scale benchmarking (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R10 demands from-scratch construction and R18 transparency; a practical implementation checklist reduces the risk of subtle omissions in our reference simplex before it becomes the baseline for R16 comparisons.
## Evidence → Engineering Decision
- *Finding:* Engineering completeness of the pivot loop determines whether comparisons are meaningful → *PS requirement:* R16 → *Component:* src/lp/reference/revised_simplex.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Dantzig-1963-Linear-Programming-Extensions]] [[Hall-2005-Hyper-sparsity-Revised-Simplex]] [[Harris-1973-Pivot-Selection-Methods]]
## Uses
- [[Revised Simplex]] [[Numerical Stability]]
