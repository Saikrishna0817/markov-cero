---
type: paper
title: "Intelligent Branching / Large-Neighborhood Branching"
authors: "Turner, Salvagnin, Coucheney et al."
year: 0000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# Intelligent Branching / Large-Neighborhood Branching

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Branching designs that go beyond a single variable: decide *where* to split using neighborhood/large-scale structure rather than one fractional column.

## Metadata
| Field | Value |
|---|---|
| Authors | Turner, Salvagnin, Coucheney et al. |
| Year | **not stated in reference list** — slug uses 0000 placeholder (see manifest) |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Single-variable binary branching is the default even when the model's structure (cliques, sets, large-neighborhood blocks) suggests a better partition of the feasible region.

## Core Contribution
- **Methodology:** Design branching rules that exploit large-neighborhood or set-based partitions ("intelligent branching"), scoring candidate branching *structures* rather than columns; aim for balanced children with strong inference.
- **Assumptions:** Structure discovery (cliques/sets) available from presolve or model analysis; scoring mechanism for structural splits.
- **Benchmarks/datasets:** MIP benchmarks (not itemized in list).
- **Metrics:** Node counts, balance of child bounds, LP time.
- **Key results:** Structured/large-neighborhood branching can outperform single-variable rules — the frontier beyond which our current rule does not reach.

## Engineering-Relevant Knowledge
**Algorithms:** Large-neighborhood branching; set/clique-based multi-way splits.
**Techniques:** Structure detection from presolve; scoring partitions by predicted child balance and bound improvement.
**Implementation details:** Requires richer presolve (clique detection) than our 4 rules provide; would also interact with parallel work distribution (child subtrees must be splittable across threads, R7).
**Equations/rules:** none transferable without the structure-detection layer.
**Limitations/failure cases:** Complexity of implementation and scoring; benefits are instance-dependent; multi-way children complicate node accounting.

## Applicability to Our Project
**Classification:** Not applicable
**Why:** Depends on presolve/structure detection we do not have; R5's "advanced node selection" is satisfiable with best-estimate selection and reliability branching first.

## Evidence → Engineering Decision
- *Finding:* structure-based branching needs clique/set presolve → *PS requirement:* R5, R6 → *Component:* src/presolve/presolve.cpp (future), src/milp/branch_selector.cpp → *Metric:* node counts on structured MIPLIB instances

## Related Papers
- [[Rader-0000-Selection-Variables-MIP]]
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Berthold-2006-Hybrid-Branching]]
- [[Achterberg-2007-Best-Estimate-Bound]]

## Uses
- [[Strong Branching]] [[Pseudo-Cost Branching]] [[Branch and Bound]] [[Cut Pooling]]
