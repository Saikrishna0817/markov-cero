---
type: paper
title: "A Practical Anti-Degeneracy Row Selection Technique in Network LP"
authors: "Maros"
year: 1993
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# A Practical Anti-Degeneracy Row Selection Technique in Network LP

> Choose *which* leaving row to prefer when several are degenerate — a cheap pivot-selection fix, demonstrated on network LPs (a common structure in logistics/planning).

## Metadata
| Field | Value |
|---|---|
| Authors | Maros |
| Year | 1993 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified); list notes "generic publisher link … need DOI lookup before download" |

## Problem Addressed
Network LPs are massively degenerate: many basic variables sit at zero, ratio tests return multiple tied candidates, and arbitrary choice causes stalling and long degenerate pivot chains.

## Core Contribution
- **Methodology:** When the ratio test offers several candidates, select the row by a criterion (rather than "first found") that avoids restarting degenerate chains — practical, tolerance-aware row selection demonstrated on network-structured LPs.
- **Assumptions:** Ratio-test candidates available; network structure (or any degenerate structure) common.
- **Benchmarks/datasets:** Network LPs (assignment/transport/planning — structures we meet in R11 logistics cases).
- **Metrics:** Degenerate pivots, iterations, time.
- **Key results:** A zero-theory, drop-in improvement: better tie-breaking in the ratio test reduces degenerate cycling at negligible cost.

## Engineering-Relevant Knowledge
**Algorithms:** Anti-degeneracy row selection inside the dual simplex ratio test.
**Techniques:** Tie-breaking among minimum reduced-cost ratios (pairs with [[Harris Ratio Test]]); prefer rows whose pivot moves the iterate off the degenerate face.
**Implementation details:** Lives in `src/lp/dual/dual_simplex.cpp` — the same component as #152; both are small pricing/ratio-test changes aimed at R13. Audit: current ratio-test/tolerance behavior unverified (TB-xx).
**Equations/rules:** Among candidates with minimal reduced-cost ratio within tolerance ε, select by secondary criterion (e.g. smallest resulting primal infeasibility).
**Limitations/failure cases:** Designed for network structure; gains may shrink on general LPs; needs tolerance-consistency to avoid false ties.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Cheapest available degeneracy mitigation in the engine used at every MIP node; fits R11 (logistics/transport structure) and R13.

## Evidence → Engineering Decision
- *Finding:* ratio-test tie-breaking not documented/verified → *PS requirement:* R13, R17 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* degenerate pivots per LP, time on network instances

## Related Papers
- [[DeFarias-2019-Positive-Edge-Pricing]]
- [[Charnes-1954-Optimality-Multi-Valuedness]]
- [[Maros-0000-New-Degeneracy-Method]]
- [[Gill-1989-Practical-Anti-Cycling]]

## Uses
- [[Harris Ratio Test]] [[Degeneracy]] [[Dual Simplex]] [[Warm Start]]
