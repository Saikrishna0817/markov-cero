---
type: paper
title: "Restart Strategies in MIP (knapsack-conflict restarts)"
authors: "Schweizer, Beringer et al."
year: 0000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# Restart Strategies in MIP (knapsack-conflict restarts)

> When to abandon a fruitless search subtree and restart with fresh pseudo-costs, a fresh node order and a new incumbent-focused strategy.

## Metadata
| Field | Value |
|---|---|
| Authors | Schweizer, Beringer et al. |
| Year | **not stated in reference list** — slug uses 0000 placeholder (see manifest) |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Long MIP searches enter regions where learned data (pseudo-costs, cut/branch history) is stale or biased by a misleading early branch, and the search keeps paying for it. SAT solvers restart routinely; MIP does not by default.

## Core Contribution
- **Methodology:** Trigger restarts on measured stagnation (bound/incumbent not improving over a window), optionally seeded by knapsack-conflict information that identifies the conflicting constraints responsible for infeasibility; on restart, clear/reweight history and reopen nodes with a different policy.
- **Assumptions:** Ability to re-open or re-solve the tree cheaply; persistent incumbents and cuts survive the restart.
- **Benchmarks/datasets:** Hard MIP instances (knapsack-structured conflicts).
- **Metrics:** Time to solution with/without restarts; number of restarts.
- **Key results:** Restarts can rescue searches stuck on an unlucky early decision — with the caveat that a badly designed restart wastes all invested tree work.

## Engineering-Relevant Knowledge
**Algorithms:** Stagnation-triggered restart; conflict-guided restart selection.
**Techniques:** Preserve incumbents and valid cuts across restarts; decay pseudo-cost history; vary node-selection order after restart.
**Implementation details:** Our `src/milp/work_queue.cpp` / `src/milp/parallel_tree_search.cpp` would need restart control points; conflicts-based triggering requires reason tracking we lack (ties to [[Jabbar-2024-Cut-Based-Conflict-Analysis]]-style machinery, not implemented).
**Equations/rules:** Restart when improvement rate Δbound/Δtime < threshold over window W (parameters instance-dependent).
**Limitations/failure cases:** Restarting without a good incumbent loses progress; interacts badly with parallel work queues unless re-balanced (R7 is already a regression at 0.56×).

## Applicability to Our Project
**Classification:** Not applicable
**Why:** Depends on mature tree infrastructure and (in its best form) conflict tracking we do not have; audit priority is fixing root cuts, heuristics and parallel overhead first.

## Evidence → Engineering Decision
- *Finding:* no restart/stagnation detection → *PS requirement:* R5, R7 → *Component:* src/milp/work_queue.cpp → *Metric:* time on stalled instances ([[Geometric Mean Runtime]])

## Related Papers
- [[Jabbar-2024-Cut-Based-Conflict-Analysis]]
- [[Achterberg-2007-Best-Estimate-Bound]]
- [[Hollenbeck-2014-Important-Branching-Decisions]]
- [[Turner-0000-Intelligent-Branching-Large]]

## Uses
- [[Branch and Bound]] [[Warm Start]] [[Pseudo-Cost Branching]] [[Parallel Speedup]]
