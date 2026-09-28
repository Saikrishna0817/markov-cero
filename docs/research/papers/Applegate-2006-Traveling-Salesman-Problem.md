---
type: paper
title: "The Traveling Salesman Problem: A Computational Study (heuristic chapters)"
authors: "Applegate, Bixby, Chvátal & Cook"
year: 2006
venue: "(not stated in list)"
doi: "(unverified)"
domain: [heuristics]
priority: ○
status: standard
tags: [paper, heuristics]
---

# The Traveling Salesman Problem: A Computational Study (heuristic chapters)

> Deep practical engineering of combinatorial heuristics (k-opt, bootstrapping, cutting planes at scale) from the Concorde authors.

## Metadata
| Field | Value |
|---|---|
| Authors | Applegate, Bixby, Chvátal & Cook |
| Year | 2006 |
| Venue | (book; not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
General MIP heuristics are thin abstractions; hard combinatorial problems need deep, structure-exploiting local search and construction methods to reach high-quality solutions at all.

## Core Contribution
- **Methodology:** Systematic development of construction heuristics, k-opt/n-opt local search, perturbation and bootstrapping, integrated with cutting-plane computation for TSP-scale instances.
- **Assumptions:** Metric structure of TSP; specialized data structures (neighborhood lists).
- **Benchmarks/datasets:** TSPLIB instances.
- **Metrics:** Tour length gap vs. best known; time to good solution.
- **Key results:** Shows how far structure-aware heuristics go beyond generic rounding — and, via Concorde, that cuts + branching remain the backbone for optimality.

## Engineering-Relevant Knowledge
**Algorithms:** Nearest-neighbor construction, 2-opt/3-opt, perturbation/restart (the local-search core behind LNS ideas).
**Techniques:** Neighborhood pruning, warm-started incumbents, interleaving heuristics with exact search.
**Implementation details:** Out of scope for our LP/MILP core (no TSP structures), but its "always have an incumbent, always improve it cheaply" discipline justifies scheduling heuristics throughout the tree rather than only at root.
**Equations/rules:** none needed for our codebase.
**Limitations/failure cases:** Highly structure-specific; not transferable as code to generic MILP.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Engineering culture reference for incumbent quality (R20); no component maps to it.

## Evidence → Engineering Decision
- *Finding:* incumbent improvement must be continuous through the search → *PS requirement:* R20 → *Component:* src/milp/heuristics.cpp, src/milp/milp_solver.cpp → *Metric:* primal integral

## Related Papers
- [[Danna-2004-Exploring-Relaxation-Induced]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Fischetti-2003-Local-Branching]]
- [[Fischetti-2005-Feasibility-Pump]]

## Uses
- [[Branch and Cut]] [[Rounding Heuristic]] [[Relative Optimality Gap]] [[MIPLIB]]
