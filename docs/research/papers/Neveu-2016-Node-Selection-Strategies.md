---
type: paper
title: "Node Selection Strategies in Interval Branch and Bound Algorithms"
authors: "Neveu, Trombettoni & Araya"
year: 2016
venue: "JGLO"
doi: "(unverified)"
domain: [branching]
priority: ✦
status: standard
tags: [paper, branching]
---

# Node Selection Strategies in Interval Branch and Bound Algorithms

> Comparative study of node-selection policies in *interval* B&B, with explicit treatment of how upper bounds (incumbents) should influence which node opens next.

## Metadata
| Field | Value |
|---|---|
| Authors | Neveu, Trombettoni & Araya |
| Year | 2016 |
| Venue | Journal of Global Optimization (list: "JGLO") |
| DOI/URL | (unverified; Springer link in list) |

## Problem Addressed
Interval branch-and-bound mixes a global lower bound with a non-monotone incumbent (upper) bound; most node-selection rules ignore how an improving incumbent changes which subtree is worth exploring.

## Core Contribution
- **Methodology:** Formalize and compare node-selection strategies in interval B&B, distinguishing policies that use only bounds, policies that use depth, and policies that explicitly incorporate the current upper bound/incumbent.
- **Assumptions:** Interval branch-and-bound engine; computable node bounds and incumbent.
- **Benchmarks/datasets:** Global-optimization test functions (not itemized in list).
- **Metrics:** Nodes, time, effect of incumbent updates on selection order.
- **Key results:** Incumbent-aware selection changes traversal materially; transferable lesson — node selection should react to incumbent improvements, not just to node bounds.

## Engineering-Relevant Knowledge
**Algorithms:** Depth-first, best-bound, incumbent-aware hybrid selection policies.
**Techniques:** Re-prioritize the queue whenever a new incumbent arrives (bound-based keys may change meaning).
**Implementation details:** Relevant to `src/milp/work_queue.cpp`: after a heuristic improves the incumbent, best-bound ordering naturally refocuses, but estimate-based ordering should be recomputed. Interval B&B is a different algorithm family — transfer the *policy* idea, not the code.
**Equations/rules:** none directly transferable.
**Limitations/failure cases:** Different problem class (continuous global optimization); numerical interval arithmetic assumptions do not hold in simplex MILP.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Supports the design claim that node selection must interact with incumbents (R5); no implementation follows from it directly.

## Evidence → Engineering Decision
- *Finding:* incumbent updates should trigger queue re-evaluation → *PS requirement:* R5 → *Component:* src/milp/work_queue.cpp, src/milp/heuristics.cpp → *Metric:* time-to-best-incumbent

## Related Papers
- [[Achterberg-2007-Best-Estimate-Bound]]
- [[Linderoth-2000-Impact-Branch-Bound]]
- [[Zhang-2025-Learning-Select-Nodes]]
- [[Danna-2004-Exploring-Relaxation-Induced]]

## Uses
- [[Branch and Bound]] [[Warm Start]] [[Relative Optimality Gap]] [[Pseudo-Cost Branching]]
