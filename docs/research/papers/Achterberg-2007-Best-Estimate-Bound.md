---
type: paper
title: "Best-estimate / best-bound node selection"
authors: "Achterberg, 2007 (thesis, §15); Linderoth & Karypis"
year: 2007
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# Best-estimate / best-bound node selection

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Node selection policies: best-bound keeps the dual bound moving (good for proof), best-estimate predicts where incumbents hide (good for pruning).

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg, 2007 (thesis, §15); Linderoth & Karypis (second, merged entry) |
| Year | 2007 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Which open node to solve next determines whether the solver proves optimality quickly or wanders. Bound-based and estimate-based policies optimize different objectives and can disagree wildly.

## Core Contribution
- **Methodology:** Define and compare: *best-bound* (open the node with the most promising LP bound — minimizes the dual gap trajectory), *best-estimate* (open the node whose estimated incumbent potential is highest, using pseudo-cost-style estimates), plus depth/hybrid variants; measure effect on time-to-incumbent vs. time-to-proof.
- **Assumptions:** Estimates available from branching history; tree with heterogeneous node bounds.
- **Benchmarks/datasets:** MIPLIB-era sets (thesis §15).
- **Metrics:** Time to first/best incumbent, time to proof, node counts.
- **Key results:** Best-bound is generally strongest overall; best-estimate helps find incumbents early. Modern solvers blend policies (best-estimate with best-bound fallback).

## Engineering-Relevant Knowledge
**Algorithms:** Best-bound, best-estimate, depth-first, hybrid node selection.
**Techniques:** Priority queue keyed by node bound; estimate of potential improvement per node.
**Implementation details:** `src/milp/branch_selector.cpp` (node selection side) / work queue in `src/milp/work_queue.cpp` must state its policy; for parallel search (`src/milp/parallel_tree_search.cpp`) the policy interacts with work stealing. Currently unreported in evidence — an easy R5/R7 win.
**Equations/rules:** best-bound: pop argmin bound (minimization); best-estimate: pop argmax(estimated improvement), fall back to bound ordering.
**Limitations/failure cases:** Pure best-bound starves incumbent search; pure best-estimate can delay the proof indefinitely; needs balancing.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** "Advanced node selection" is named in R5 and we must document (and measure) our current policy before claiming it.

## Evidence → Engineering Decision
- *Finding:* node selection policy unreported in our evidence → *PS requirement:* R5, R7 → *Component:* src/milp/work_queue.cpp, src/milp/parallel_tree_search.cpp → *Metric:* time-to-incumbent, total time, [[Geometric Mean Runtime]]

## Related Papers
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Linderoth-2000-Impact-Branch-Bound]]
- [[Schweizer-0000-Restart-Strategies-MIP]]
- [[Zhang-2025-Learning-Select-Nodes]]

## Uses
- [[Branch and Bound]] [[Pseudo-Cost Branching]] [[Relative Optimality Gap]] [[Parallel Speedup]]
