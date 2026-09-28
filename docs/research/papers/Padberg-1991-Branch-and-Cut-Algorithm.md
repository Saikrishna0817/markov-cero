---
type: paper
title: "A Branch-and-Cut Algorithm for the Resolution of Large-Scale Symmetric TSP Problems"
authors: "Padberg & Rinaldi"
year: 1991
venue: "SIAM Review"
doi: "(unverified)"
domain: [milp]
priority: ★
status: deep
tags: [paper, milp]
---

# A Branch-and-Cut Algorithm for the Resolution of Large-Scale Symmetric TSP Problems

> The branch-and-cut paper: LP relaxation + cutting-plane separation + combinatorial bounding inside one search.

## Metadata
| Field | Value |
|---|---|
| Authors | Padberg & Rinaldi |
| Year | 1991 |
| Venue | SIAM Review |
| DOI/URL | (unverified) |

## Problem Addressed
Pure branch-and-bound on weak relaxations of the TSP is hopeless; pure cutting planes take enormous numbers of them. Padberg & Rinaldi combined separation *within* the tree: solve node LP, separate violated cuts, iterate, then branch only when separation fails — and pruned entire regions combinatorially.

## Core Contribution
- **Methodology:** Node LP solve -> cut separation loop (subtour/comb inequalities) with rounding; combinatorial bounding (spanning-tree/degree arguments) for pruning without LP; branching only on unresolved fractional solutions; extensive incumbent heuristics.
- **Assumptions:** Valid inequalities that preserve all integer feasible solutions; separation heuristic quality; LP solver producing optimal vertex solutions.
- **Benchmarks/datasets:** Large symmetric TSP instances (up to thousands of cities in the paper).
- **Metrics:** LP iterations, cuts per node, nodes, total time, gap closed at root.
- **Key results:** Solved instances previously out of reach; demonstrated that cut separation inside the tree is what makes large MIPs tractable (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut; cut separation at nodes; combinatorial bounding.
**Techniques:** Root-level cut rounds; cut management (which cuts to keep); heuristic incumbents to enable pruning.
**Implementation details:** Our tree (`src/milp/milp_solver.cpp`) already separates GMI/MIR cuts (`src/milp/gomory.cpp`, `src/milp/mir.cpp` into `src/milp/cut_pool.cpp`) — but root-only. Padberg-Rinaldi is the argument for per-node separation and for periodic re-optimization rounds.
**Limitations/failure cases:** Cut explosion without management ([[Cut Efficiency]]); degenerate node LPs slow separation ([[Degeneracy]]).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 (branch-and-cut + cutting planes) and R20. The paper defines the loop we partially implement; extending cuts beyond the root is the direct follow-on.

## Evidence → Engineering Decision
- *Finding:* Separation must continue at nodes, not stop at the root → *PS requirement:* R5 → *Component:* src/milp/cut_pool.cpp + src/milp/node_lp.cpp (per-node separation hook) → *Metric:* [[Cut Efficiency]]; nodes explored; root vs. tree gap closed.
- *Finding:* Combinatorial bounds prune without LP solves → *PS requirement:* R20 → *Component:* src/milp/milp_solver.cpp → *Metric:* LP iterations per node.

## Related Papers
- [[Achterberg-2005-General-Mixed-Integer]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Land-1960-Automatic-Method-Solving]]
- [[Cornuejols-2001-Branch-and-Cut-Algorithms]]

## Uses
- [[Branch and Cut]]
- [[Cut Validity]]
- [[LP Relaxation]]
- [[Weak Relaxation]]
