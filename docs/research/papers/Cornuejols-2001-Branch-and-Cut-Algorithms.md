---
type: paper
title: "Branch-and-Cut Algorithms for Combinatorial Optimization and Their Implementation in ABACUS"
authors: "Cornuejols & Li"
year: 2001
venue: "Springer chapter (as listed)"
doi: "10.1007/3-540-45586-8_5 (derived from list link)"
domain: [milp]
priority: ○
status: standard
tags: [paper, milp]
---
# Branch-and-Cut Algorithms for Combinatorial Optimization and Their Implementation in ABACUS
> Software architecture of branch-and-cut: object-oriented managers for cuts, columns and callbacks (ABACUS).
## Metadata
| Field | Value |
|---|---|
| Authors | Cornuejols & Li |
| Year | 2001 |
| Venue | Springer chapter (as listed) |
| DOI/URL | 10.1007/3-540-45586-8_5 |
## Problem Addressed
Algorithms papers say "add a cut" but not how a codebase should organize cut pools, lifetime management, node callbacks and user extension points. ABACUS demonstrates a reusable object-oriented architecture for branch-and-cut.
## Core Contribution
- **Methodology:** Class design: problem interface, cut management (pool with selection/aging), node processing callbacks, separation hooks, user-defined branching/selection policies.
- **Assumptions:** C++-style OOP (era-appropriate); cuts exchangeable; node state encapsulated.
- **Benchmarks/datasets:** Combinatorial problems implemented on ABACUS (qualitative).
- **Metrics:** Engineering metrics (extensibility) plus solve performance.
- **Key results:** Demonstrated that a generic framework can host diverse combinatorial problems (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut as a framework, not a monolith.
**Techniques:** Cut pool with selection/aging; callback-style extension points.
**Implementation details:** Our `src/milp/cut_pool.cpp` is the seed of the ABACUS cut manager: selection policy, capacity limits and aging are missing. R1 (solver core, not modeling environment) means extension points should be C++ interfaces, not user callbacks in a DSL.
**Limitations/failure cases:** Over-abstraction costs performance; 2001 design predates parallel tree search (our `src/milp/parallel_tree_search.cpp` must own node state, not callbacks).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R1, R5, R18 (extensible architecture): blueprint for cut-pool lifecycle and node-processing interfaces as our MILP engine grows.
## Evidence → Engineering Decision
- *Finding:* Cut pools need selection/aging, not FIFO accumulation → *PS requirement:* R5 → *Component:* src/milp/cut_pool.cpp → *Metric:* [[Cut Efficiency]]; pool memory bound.
## Related Papers
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Junger-1995-Branch-and-Cut-Combinatorial]]
## Uses
- [[Branch and Cut]]
- [[Cut Validity]]
