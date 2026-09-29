---
type: paper
title: "Integer and Combinatorial Optimization"
authors: "Nemhauser & Wolsey"
year: 1988
venue: "Book"
doi: "(unverified)"
domain: [survey, milp]
priority: ○
status: standard
tags: [paper, survey, milp]
---
# Integer and Combinatorial Optimization

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The standard theoretical treatment of LP, polyhedral combinatorics and branch-and-bound for integer programs.
## Metadata
| Field | Value |
|---|---|
| Authors | Nemhauser & Wolsey |
| Year | 1988 (approximate — no year in reference list) |
| Venue | Book |
| DOI/URL | (unverified) |
## Problem Addressed
Integer programs mix combinatorial difficulty with rich LP structure; the field needed one rigorous text deriving relaxation, duality, valid inequalities and branch-and-bound from common foundations rather than scattered papers.
## Core Contribution
- **Methodology:** Graduate-level text: theorems and proofs building from LP duality through cutting planes, branch-and-bound, branch-and-cut and Lagrangian methods.
- **Assumptions:** Exact mathematical treatment; pre-modern-solver era (year approximate).
- **Benchmarks/datasets:** None (theory text).
- **Metrics:** Not applicable.
- **Key results:** Canonical presentation of the LP-relaxation and valid-inequality framework every MILP solver rests on (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** LP duality, cutting-plane generation, branch-and-bound, Lagrangian relaxation.
**Techniques:** Valid/facet-defining inequalities, bounding, relaxation hierarchy.
**Implementation details:** Supplies the correctness arguments (cut validity, finiteness of branch-and-bound, correctness of bounding) that must hold in src/milp/gomory.cpp, src/milp/mir.cpp and src/milp/branch_selector.cpp.
**Equations/rules:** An optimal integer point never beats the LP relaxation bound; integrality gap = (integer obj − LP bound)/|LP bound| (Relative Optimality Gap).
**Limitations/failure cases:** No computational or numerical guidance; formulations predate modern sparse numerics (approximate).
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Theory backbone for R2/R5; R10 forbids reusing solver libraries but not mathematical results, so cut-validity and bounding arguments from this text are the proof layer behind our MILP code.
## Evidence → Engineering Decision
- *Finding:* Valid cuts and finite search require a correct LP relaxation bound at every node → *PS requirement:* R5 → *Component:* src/milp/gomory.cpp → *Metric:* Relative Optimality Gap
## Related Papers
- [[Achterberg-2007-Constraint-Integer-Programming]] [[Kumar-2010-Fifty-Years-Integer]] [[Hoffman-1991-Improving-LP-Representations]]
## Uses
- [[Cut Validity]] [[LP Relaxation]]
