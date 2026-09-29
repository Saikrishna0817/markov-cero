---
type: paper
title: "A Fast and High Quality Multilevel Scheme for Partitioning Irregular Graphs"
authors: "Karypis & Kumar"
year: 1998
venue: "(unverified)"
doi: "(unverified)"
domain: [sparse]
priority: ○
status: standard
tags: [paper, sparse]
---

# A Fast and High Quality Multilevel Scheme for Partitioning Irregular Graphs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> METIS: multilevel coarsen–partition–refine graph partitioning, the template for nested-dissection orderings on very large systems.
## Metadata
| Field | Value |
|---|---|
| Authors | Karypis & Kumar |
| Year | 1998 |
| Venue | (unverified) |
| DOI/URL | (unverified) |
## Problem Addressed
Very large sparse systems need orderings/partitions whose quality is far better than greedy minimum degree can deliver at acceptable cost; the paper gives the multilevel paradigm (coarsen, partition the tiny graph, project back and refine) that achieves near-optimal cuts quickly.
## Core Contribution
- **Methodology:** Recursive graph coarsening via heavy-edge matching, bisection of the coarsest graph, then uncoarsening with local refinement (KL/FM-style) at each level.
- **Assumptions:** Problem represented as an irregular graph; edge weights capture coupling strength (approximate).
- **Benchmarks/datasets:** Large irregular graphs from finite-element/sparse-matrix applications (qualitative; no figures asserted).
- **Metrics:** Edge-cut quality, partitioning time, speedup of downstream computations (qualitative).
- **Key results:** Near-optimal partitions at very low cost; became the standard multilevel partitioning scheme (qualitative/approximate).
## Engineering-Relevant Knowledge
**Algorithms:** Multilevel graph partitioning used to derive nested-dissection fill-reducing orderings.
**Techniques:** Fill-Reducing Ordering via nested dissection, heavy-edge matching coarsening, local refinement, edge-cut balancing.
**Implementation details:** Applicable at the extreme scale end of R12 (millions of rows) where AMD/COLAMD ordering cost grows; useful for any block-parallel factorization (R7) — but only if measurement shows it beats cheaper orderings on our benchmark set.
**Equations/rules:** Cut(A,B) = Σ weights of edges crossing the partition; ordering = recursive bisection with separators eliminated last.
**Limitations/failure cases:** Overkill for small/medium bases; quality depends on weighting heuristics; partitioning time can dominate if applied naively per refactorization (approximate).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R12 targets million-variable sparse models and R7 wants parallelism; nested dissection gives both better fill and natural parallel blocks, but only at scale — an empirical threshold we should measure rather than assume (R16/R20).
## Evidence → Engineering Decision
- *Finding:* Multilevel ordering pays off only beyond a size threshold → *PS requirement:* R12 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* Geometric Mean Runtime
## Related Papers
- [[Amestoy-1996-Approximate-Minimum-Degree]] [[Duff-1986-Direct-Methods-Sparse]] [[Liu-1990-Role-Elimination-Trees]]
## Uses
- [[Fill-Reducing Ordering]] [[Sparsity]]
