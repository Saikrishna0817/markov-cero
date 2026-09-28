---
type: paper
title: "Branching Rules Revisited"
authors: "Achterberg, Koch & Martin"
year: 2005
venue: "OR Letters"
doi: "(unverified)"
domain: [branching]
priority: ★
status: deep
tags: [paper, branching]
---

# Branching Rules Revisited

> Systematic comparison of branching rules — most-infeasible, pseudo-cost, strong, reliability — with the reliability parameters (η_rel = 8, λ = 4) now standard in every solver.

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg, Koch & Martin |
| Year | 2005 |
| Venue | Operations Research Letters (list: "OR Letters") |
| DOI/URL | (unverified; PDF link in reference list) |

## Problem Addressed
Branching rules are usually compared anecdotally on a few instances. Without a controlled comparison of node count, LP time and reliability overhead, implementers cannot justify a default.

## Core Contribution
- **Methodology:** Implement most-infeasible, pseudo-cost, strong and reliability branching in one engine; measure node counts and time; define reliability branching = probe a candidate only until its pseudo-cost estimates are trustworthy (η_rel probes, confidence λ).
- **Assumptions:** Pseudo-cost history initialized (from prior probes or symmetry); LP solver available for trial branches; branching candidates screened first.
- **Benchmarks/datasets:** MIPLIB 2003-era set.
- **Metrics:** Nodes, total time, LP time share, probe overhead.
- **Key results:** Reliability/pseudo-cost branching dominates most-infeasible on node count at modest overhead; supplies the concrete defaults η_rel = 8 and λ = 4 recorded in our reference list.

## Engineering-Relevant Knowledge
**Techniques:** [[Pseudo-Cost Branching]], [[Strong Branching]], reliability branching, most-infeasible rule, candidate screening.
**Implementation details:** We implement strong + reliability pseudo-costs (`src/milp/strong_branching.cpp`, `src/milp/branch_selector.cpp`). The paper's probe counts (η_rel=8, λ=4) are directly comparable to our probe budget — audit them as tunables, not constants.
**Equations/rules:** Pseudo-cost down/up Δ_j estimated from probe(s); reliable if all candidates probed ≥ η_rel times or confidence λ met; otherwise strong-branch the unprobed candidates.
**Limitations/failure cases:** Pseudo-costs initialized badly on symmetric instances (both directions equal) ⇒ misleading; probing costs LP solves — on cheap nodes it can dominate runtime.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** This is the spec for the branching code we already ship; it defines the tuning parameters and the measurement protocol for R5's "advanced node selection".

## Evidence → Engineering Decision
- *Finding:* we have strong/reliability branching but no published parameter justification → *PS requirement:* R5, R16 → *Component:* src/milp/strong_branching.cpp, src/milp/branch_selector.cpp → *Metric:* node count, LP time share, [[Geometric Mean Runtime]]

## Related Papers
- [[Linderoth-2000-Impact-Branch-Bound]]
- [[Held-2006-Lookahead-Branching-Mixed]]
- [[Driebeek-1966-Algorithm-Assignment-Problem]]
- [[Achterberg-2007-Best-Estimate-Bound]]

## Uses
- [[Strong Branching]] [[Pseudo-Cost Branching]] [[Branch and Bound]] [[LP Relaxation]]
