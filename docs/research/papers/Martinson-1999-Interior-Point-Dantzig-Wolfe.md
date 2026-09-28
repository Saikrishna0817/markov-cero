---
type: paper
title: "An Interior Point Method in Dantzig-Wolfe Decomposition"
authors: "Martinson & Tind"
year: 1999
venue: "Comput. Optim. Appl."
doi: "(unverified)"
domain: [ipm]
priority: ✦
status: standard
tags: [paper, ipm]
---
# An Interior Point Method in Dantzig-Wolfe Decomposition
> Interior-point method that follows the block-angular structure of Dantzig-Wolfe decompositions instead of destroying it.
## Metadata
| Field | Value |
|---|---|
| Authors | Martinson & Tind |
| Year | 1999 |
| Venue | Comput. Optim. Appl. |
| DOI/URL | (unverified) |
## Problem Addressed
Large planning LPs are naturally block-angular (coupling rows + independent blocks — refinery plans, multi-period production). Monolithic IPM factorizations ignore that structure and fill in badly. The paper embeds IPM inside Dantzig-Wolfe decomposition to exploit it.
## Core Contribution
- **Methodology:** Dantzig-Wolfe reformulation + column generation structure carried into the primal-dual path-following iteration; block systems solved independently, master step couples them.
- **Assumptions:** Block-angular LP; bounded subproblem polyhedra (pricing oracle terminates); blocks sparse and separable.
- **Benchmarks/datasets:** Decomposable test LPs (paper's set).
- **Metrics:** Iterations, block-solve cost, scaling with block size.
- **Key results:** Structural exploitation lowers per-iteration cost on block models (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Dantzig-Wolfe decomposition with interior-point iterations.
**Techniques:** Block-parallel subproblem solves; master/slave linear algebra split.
**Implementation details:** Blocks map naturally onto multi-core (R7: `src/milp/parallel_tree_search.cpp` shows the threading style); no decomposition code exists in markov-cero.
**Limitations/failure cases:** Degenerate masters slow the master step; column generation oscillation carries over to IPM steps.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R11 scope (refinery scheduling, production planning) and R12 (large sparse models) are dominated by block-angular structure; useful design reading for a future decomposition extension, not for current code.
## Evidence → Engineering Decision
- *Finding:* Block structure can be preserved through IPM iterations → *PS requirement:* R11, R7 → *Component:* src/milp/parallel_tree_search.cpp (threading patterns to reuse) → *Metric:* per-iteration time on block-structured instances.
## Related Papers
- [[Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Kojima-1989-Primal-Dual-Interior-Point-Algorithm]]
## Uses
- [[Interior-Point Method]]
- [[Sparsity]]
