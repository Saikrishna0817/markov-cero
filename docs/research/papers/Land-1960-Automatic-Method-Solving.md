---
type: paper
title: "An Automatic Method of Solving Discrete Programming Problems"
authors: "Land & Doig"
year: 1960
venue: "Econometrica"
doi: "(unverified)"
domain: [milp]
priority: ★
status: deep
tags: [paper, milp]
---

# An Automatic Method of Solving Discrete Programming Problems

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The original branch-and-bound: recursive partitioning with LP bounds to solve discrete optimization automatically.

## Metadata
| Field | Value |
|---|---|
| Authors | Land & Doig |
| Year | 1960 |
| Venue | Econometrica |
| DOI/URL | (unverified) |

## Problem Addressed
Discrete (integer) programming had no systematic solution method; exhaustive enumeration explodes combinatorially. Land & Doig introduced the scheme that underlies every MIP solver: branch on a variable, bound each child with its LP relaxation, prune what cannot beat the incumbent.

## Core Contribution
- **Methodology:** Tree search over variable domains; at each node solve an LP relaxation; if integral, update incumbent; if fractional, branch (split domain); prune nodes whose bound is no better than the incumbent.
- **Assumptions:** Minimization with valid LP bounds (relaxation property); bounded domains; exact LP solves at nodes.
- **Benchmarks/datasets:** Small discrete economic planning examples (paper's set).
- **Metrics:** Nodes explored; bound quality; optimality gap closed.
- **Key results:** Automatic solution of problems previously handled by ad hoc enumeration (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-bound; LP-relaxation bounding.
**Techniques:** Incumbent management; node pruning by bound comparison.
**Implementation details:** Structure exists in `src/milp/milp_solver.cpp` (node queue, incumbents, `src/milp/work_queue.cpp`); Land-Doig is the correctness baseline for the prune test `node_bound >= incumbent - tol`.
**Equations/rules:** prune iff `bound(node) >= incumbent - feasibility_tolerance` (minimization).
**Limitations/failure cases:** Useless if bounds are weak ([[Weak Relaxation]]) — motivates presolve, cuts and strong branching; sensitive to branching order.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 names branch-and-bound explicitly; the pruning/bounding contract is the invariant every feature (cuts, heuristics, node selection) must preserve.

## Evidence → Engineering Decision
- *Finding:* Tree size is driven by bound quality, not search mechanics → *PS requirement:* R5, R20 → *Component:* src/milp/milp_solver.cpp (bound & prune logic) → *Metric:* nodes explored; [[Relative Optimality Gap]] at termination.

## Related Papers
- [[Balas-1965-Additive-Algorithm-Solving]]
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Achterberg-2005-General-Mixed-Integer]]

## Uses
- [[Branch and Bound]]
- [[LP Relaxation]]
- [[Relative Optimality Gap]]
