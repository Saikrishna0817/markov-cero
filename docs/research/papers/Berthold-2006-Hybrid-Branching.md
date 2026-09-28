---
type: paper
title: "Hybrid Branching"
authors: "Berthold"
year: 2006
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ○
status: standard
tags: [paper, branching]
---

# Hybrid Branching

> Combines MIP reliability branching with CP inference scoring and SAT-style activity scores — SCIP's default branching cascade.

## Metadata
| Field | Value |
|---|---|
| Authors | Berthold |
| Year | 2006 |
| Venue | (not stated in reference list; ZIB paper PDF in list) |
| DOI/URL | (unverified; PDF link in list) |

## Problem Addressed
No single score ranks branching candidates well: LP-based pseudo-costs miss inference (bound propagation), and inference scores miss LP dual effects. A cascade that uses whichever signal is cheapest first is needed.

## Core Contribution
- **Methodology:** Score candidates in tiers — reliability (pseudo-cost) branching, constraint-propagation inference scoring, SAT/VSIDS-style activity — falling through to strong branching when confidence is low.
- **Assumptions:** Component scores available (LP history, propagator, activity counters); screening reduces the candidate set first.
- **Benchmarks/datasets:** SCIP-era MIP/CP test instances (not itemized in list).
- **Metrics:** Nodes, time, score-computation overhead.
- **Key results:** Hybrid scoring beats any single rule — the design that became SCIP's default and the reference for `branch_selector.cpp`-style code.

## Engineering-Relevant Knowledge
**Techniques:** Candidate screening, tiered scoring, fallback to [[Strong Branching]] on low confidence.
**Implementation details:** We have a `src/milp/branch_selector.cpp` with LP-based scores only; we have no propagator, so the inference tier is unavailable — the honest subset is pseudo-cost + strong-branch fallback (already close to our current design).
**Equations/rules:** rank candidates by available score; if confidence < threshold, probe with LP (reliability rule from #127).
**Limitations/failure cases:** Tiers need tuning; without a propagator, the CP/SAT tiers are simply absent (as in our solver).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Documents the full cascade so we can state explicitly which tiers we support and which are out of scope (no CP propagator in a from-scratch LP/MILP core, R10/R3).

## Evidence → Engineering Decision
- *Finding:* only LP-based scoring exists → *PS requirement:* R5 → *Component:* src/milp/branch_selector.cpp → *Metric:* nodes, scoring overhead

## Related Papers
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Driebeek-1966-Algorithm-Assignment-Problem]]
- [[Held-2006-Lookahead-Branching-Mixed]]
- [[Berthold-2013-Cloud-Branching]]

## Uses
- [[Pseudo-Cost Branching]] [[Strong Branching]] [[Branch and Bound]] [[LP Relaxation]]
