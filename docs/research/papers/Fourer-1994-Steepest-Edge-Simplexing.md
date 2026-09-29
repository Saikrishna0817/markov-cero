---
type: paper
title: "Steepest-Edge Simplexing for Network-Style Problems"
authors: "Fourer"
year: 1994
venue: "Networks"
doi: "(unverified)"
domain: [lp]
priority: ○
status: standard
tags: [paper, lp]
---

# Steepest-Edge Simplexing for Network-Style Problems

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> BFRT and steepest-edge details specialized to network-style LP structures.
## Metadata
| Field | Value |
|---|---|
| Authors | Fourer |
| Year | 1994 |
| Venue | Networks |
| DOI/URL | (unverified) |
## Problem Addressed
Network-style LPs (transport, assignment, flow models — a core industrial class for R11) have special structure that generic simplex pricing and bound handling exploit poorly: many variables sit at bounds and degenerate pivots abound. The paper studies how steepest-edge pricing and bound-flipping (BFRT) behave on these problems.
## Core Contribution
- **Methodology:** Analysis and adaptation of steepest-edge simplex pricing and bound-flipping row techniques for network-structured LPs (approximate).
- **Assumptions:** Network-style (sparse, bounded, highly degenerate) structure; standard simplex framework (approximate).
- **Benchmarks/datasets:** Network LPs (qualitative; no instance list asserted).
- **Metrics:** Iterations, pivots per bound flip, runtime (qualitative).
- **Key results:** Bound-flipping and geometric pricing behave differently — and better — on network-style degeneracy (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Steepest-edge simplex with BFRT bound flipping.
**Techniques:** BFRT details (multiple bound flips per iteration), pricing weight maintenance on network degeneracy.
**Implementation details:** Transportation/assignment models are in our R11 scope; when our src/lp/dual/dual_simplex.cpp gains BFRT, network instances are the natural first validation set alongside degenerate MIPLIB relaxations.
**Equations/rules:** When a bound blocks the dual step, flip the bound and continue the same iteration (bound flipping) instead of pivoting trivially (Dual Simplex).
**Limitations/failure cases:** Findings are structure-specific; generic models may not see the same gains (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Network/transportation problems are explicitly in R11's scope and are classic degeneracy sources (R13); BFRT tuned on these gives a concrete acceptance test for our dual simplex upgrades (R15/R17).
## Evidence → Engineering Decision
- *Finding:* Bound-flipping addresses the degenerate pivots typical of network-style models → *PS requirement:* R13 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Goldfarb-1992-Steepest-Edge-Simplex]] [[Koberstein-2005-Dual-Simplex-Method]] [[Fourer-n.d.-Hierarchical-Solution-Large]]
## Uses
- [[Steepest Edge]] [[Degeneracy]]
