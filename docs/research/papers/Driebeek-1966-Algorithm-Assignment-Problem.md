---
type: paper
title: "An Algorithm for the Assignment Problem (pseudocost origins)"
authors: "Driebeek"
year: 1966
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# An Algorithm for the Assignment Problem (pseudocost origins)

> The earliest known use of pseudo-costs: estimate a variable's branching impact from its past behavior instead of re-solving.

## Metadata
| Field | Value |
|---|---|
| Authors | Driebeek |
| Year | 1966 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Strong branching evaluates every candidate by trial LP solves; for the assignment problem this was already too expensive in 1966. A cheap estimate of "how much will this variable's bound change improve the objective?" was needed.

## Core Contribution
- **Methodology:** Track per-variable history of objective change per unit bound change (up/down), and use those pseudo-costs to score candidates without trial solves.
- **Assumptions:** History from earlier branches is predictive; assignment-problem structure.
- **Benchmarks/datasets:** Assignment problems.
- **Metrics:** Objective improvement per probe avoided; solve time.
- **Key results:** Origin of pseudo-cost branching — the estimator behind reliability branching (#127) and every modern default rule.

## Engineering-Relevant Knowledge
**Algorithms:** [[Pseudo-Cost Branching]] estimator; up/down cost separation.
**Techniques:** Initialize pseudo-costs from a few probes (reliability), then extrapolate; fall back to strong branching when confidence is low.
**Implementation details:** Our implementation (`src/milp/strong_branching.cpp`) already maintains pseudo-costs — this note documents their origin and the up/down asymmetry requirement (never merge up and down estimates).
**Equations/rules:** Δ_j^up = (z_child − z_parent)/(increase in x_j lower bound); analogous Δ_j^down; candidate score = Σ over fractional part weights.
**Limitations/failure cases:** Cold start (no history) gives equal/meaningless estimates; history from a different part of the tree may not transfer.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Historical provenance of code we already have; useful for audit narrative (R5) but requires no change.

## Evidence → Engineering Decision
- *Finding:* pseudo-costs need cold-start handling → *PS requirement:* R5 → *Component:* src/milp/strong_branching.cpp → *Metric:* nodes after probing overhead

## Related Papers
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Held-2006-Lookahead-Branching-Mixed]]
- [[Berthold-2006-Hybrid-Branching]]
- [[Linderoth-2000-Impact-Branch-Bound]]

## Uses
- [[Pseudo-Cost Branching]] [[Strong Branching]] [[Branch and Bound]] [[Warm Start]]
