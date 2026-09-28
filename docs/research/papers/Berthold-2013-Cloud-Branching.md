---
type: paper
title: "Cloud Branching"
authors: "Berthold & Salvagnin"
year: 2013
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# Cloud Branching

> Branches over the *set* of alternative LP optima ("the cloud") instead of one chosen vertex — turning degeneracy from a nuisance into information.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold & Salvagnin |
| Year | 2013 |
| Venue | (not stated in reference list; PDF link in list) |
| DOI/URL | (unverified; PDF link in list) |

## Problem Addressed
On degenerate LP relaxations many variables sit at a bound at *every* optimal vertex, so branching on them is futile; meanwhile the LP has a whole optimal face and the chosen basis hides that structure.

## Core Contribution
- **Methodology:** Sample alternative optimal vertices/bases (the "cloud") and score branching candidates by how often they are fractional *across* the cloud, rather than at a single arbitrary optimum.
- **Assumptions:** Ability to enumerate/sample alternative optima cheaply (dual optimal face); degenerate instances.
- **Benchmarks/datasets:** Degenerate MIP instances (network-style, set covering).
- **Metrics:** Node counts on degenerate instances; sampling overhead.
- **Key results:** Improves branching decisions exactly where classical pseudo-costs are misleading — degenerate models, which the PS explicitly calls out.

## Engineering-Relevant Knowledge
**Algorithms:** Cloud branching; alternative-optima sampling for candidate scoring.
**Techniques:** Perturbing the dual objective / cycling through alternative bases to explore the optimal face; combining cloud scores with pseudo-costs.
**Implementation details:** Requires an LP engine able to enumerate alternative optima — our [[Dual Simplex]] (`src/lp/dual/dual_simplex.cpp`) can re-optimize on a perturbed objective to generate samples. Directly targets R13 (degenerate models).
**Equations/rules:** score(j) = fraction of sampled optima where x_j is fractional (plus pseudo-cost blend).
**Limitations/failure cases:** Sampling costs extra LP solves; little value when the LP is nondegenerate; depends on tolerances that distinguish "alternative optimum" from "different point".

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** It is the branching-side answer to degeneracy — one of the three PS robustness keywords (R13) — and can reuse our existing dual simplex rather than new machinery.

## Evidence → Engineering Decision
- *Finding:* degenerate LPs make single-optimum pseudo-costs unreliable → *PS requirement:* R13, R5 → *Component:* src/milp/branch_selector.cpp, src/lp/dual/dual_simplex.cpp → *Metric:* nodes on degenerate MIPLIB instances

## Related Papers
- [[Charnes-1954-Optimality-Multi-Valuedness]]
- [[Maros-1993-Practical-Anti-Degeneracy]]
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Berthold-2006-Hybrid-Branching]]

## Uses
- [[Degeneracy]] [[Basis]] [[Strong Branching]] [[Pseudo-Cost Branching]]
