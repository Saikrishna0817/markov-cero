---
type: paper
title: "Hierarchical Solution of Large-Scale Linear Programs"
authors: "Fourer & Maros"
year: "n.d."
venue: "(unverified)"
doi: "(unverified)"
domain: [lp]
priority: ○
status: standard
tags: [paper, lp]
---

# Hierarchical Solution of Large-Scale Linear Programs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Applies hierarchical/decomposition structure to very large LPs instead of one monolithic solve (year not given in source list).
## Metadata
| Field | Value |
|---|---|
| Authors | Fourer & Maros |
| Year | n.d. (not given in source list) |
| Venue | (unverified) |
| DOI/URL | (unverified) |
## Problem Addressed
Very large LPs (block-structured models as arise in planning, scheduling and network design) are expensive to solve as a single dense-sparse system; the paper explores solving them hierarchically so structure at several scales is exploited rather than flattened.
## Core Contribution
- **Methodology:** Hierarchical solution scheme for large-scale LPs, organizing computation by structural levels (approximate; source entry gives only title and authors).
- **Assumptions:** Large-scale LP with exploitable hierarchical/block structure (approximate).
- **Benchmarks/datasets:** Large-scale LPs (qualitative; no instance list asserted).
- **Metrics:** Solution time vs. monolithic solve (qualitative).
- **Key results:** Structure-aware hierarchical treatment is competitive for large-scale LPs (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Hierarchical LP solution; relates to decomposition and warm-started re-optimization.
**Techniques:** Level-structured computation, reuse of intermediate bases/bounds between levels.
**Implementation details:** Structural decomposition is out of scope for our core (R1), but the idea supports our node-LP reuse: solve structure once, reuse factors/bounds across related solves (warm starts at MIP nodes).
**Equations/rules:** Blocks solved in dependency order; aggregates passed upward (qualitative).
**Limitations/failure cases:** Metadata incomplete in the source list (title/author only — year and venue unverified); generic hierarchy detection is itself a hard problem (approximate).
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Confirms that structure-aware organization matters at industrial scale (R11/R12); actionable version for us is warm-start/reuse across node LPs (R5) rather than a new decomposition engine.
## Evidence → Engineering Decision
- *Finding:* Reusing structure across related solves reduces total cost at scale → *PS requirement:* R12 → *Component:* src/milp/node_lp.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Fourer-1994-Steepest-Edge-Simplexing]] [[Fourer-1982-Solving-Linear-Programs]] [[Gondzio-1994-Another-Simplex-Type]]
## Uses
- [[Warm Start]] [[Sparsity]]
