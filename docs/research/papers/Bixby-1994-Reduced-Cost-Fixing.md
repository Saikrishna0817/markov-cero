---
type: paper
title: "Reduced Cost Fixing"
authors: "Bixby, 1994; Savelsbergh, 1994"
year: 1994
venue: "(not stated in list)"
doi: "(unverified)"
domain: [heuristics]
priority: ○
status: standard
tags: [paper, heuristics]
---

# Reduced Cost Fixing

> Fix variables to their bounds using LP [[Reduced Cost]] values plus an incumbent bound — free presolve-style pruning everywhere in the tree.

## Metadata
| Field | Value |
|---|---|
| Authors | Bixby (1994); Savelsbergh (1994) — merged entry in list |
| Year | 1994 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Given an LP optimum and a good incumbent, many variables are provably unable to improve the objective; leaving them free wastes branching effort and keeps relaxations weak.

## Core Contribution
- **Methodology:** From LP duals and the incumbent objective z_inc, compute per-variable bounds on the objective change allowed (tolerance = z_inc − z_LP); any variable whose reduced cost exceeds that tolerance (scaled by its bound) can be fixed to its LP/bound value.
- **Assumptions:** LP optimum available (dual feasible within tolerance); incumbent exists; bounded variables (or bounded implied bounds).
- **Benchmarks/datasets:** Network/MIP instances (Bixby; Savelsbergh's network-oriented work).
- **Metrics:** Variables fixed, nodes saved, time reduction.
- **Key results:** One of the largest practical speedups in branch-and-bound; nearly free given LP duals.

## Engineering-Relevant Knowledge
**Algorithms:** Reduced-cost fixing at root and at nodes; implied-bound tightening from duals.
**Techniques:** Tolerance-aware fixing (never fix on a numerically marginal reduced cost — ties to LP tolerances); re-run after incumbent improvement.
**Implementation details:** We have `src/presolve/presolve.cpp` (4 rules) but **no reduced-cost fixing**; adding it needs dual values from `src/lp/dual/dual_simplex.cpp` plus an incumbent — all available. Cheap, high-leverage.
**Equations/rules:** For min c: if x_j at lower bound and rc_j = c_j − y^T A_j > (z_inc − z_LP)/u_j (with appropriate sign/bound case), fix x_j = l_j; analogous for upper bounds.
**Limitations/failure cases:** Meaningless with only a weak incumbent or a far-from-optimal z_LP; unsafe if reduced costs are computed from an ill-conditioned basis (see #144/#147).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Missing, cheap, and it converts incumbent quality into tree reduction — the mechanism behind most of the "heuristics prune the tree" effect (R5, R20).

## Evidence → Engineering Decision
- *Finding:* no reduced-cost fixing in presolve (4 rules only) → *PS requirement:* R5, R20 → *Component:* src/presolve/presolve.cpp, src/lp/dual/dual_simplex.cpp → *Metric:* nodes fixed, [[Relative Optimality Gap]]

## Related Papers
- [[Linderoth-2000-Impact-Branch-Bound]]
- [[Danna-2004-Exploring-Relaxation-Induced]]
- [[Renegar-1994-Condition-Numbers-Linear]]
- [[Gleixner-2015-Iterative-Refinement-Linear]]

## Uses
- [[Reduced Cost]] [[LP Relaxation]] [[Duality Gap]] [[Basis]]
