---
type: paper
title: "Lookahead Branching for Mixed Integer Programming"
authors: "Held, Savelsbergh & Woodruff"
year: 2006
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ★
status: deep
tags: [paper, branching]
---

# Lookahead Branching for Mixed Integer Programming

> Two-level lookahead: after a trial branch, branch one level deeper in the trial children before committing, using the implications observed.

## Metadata
| Field | Value |
|---|---|
| Authors | Held, Savelsbergh & Woodruff |
| Year | 2006 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified; PDF link in list) |

## Problem Addressed
One-step branching (strong branching) evaluates each candidate by one LP solve per child, but the real signal is what happens *after* the child is branched — a candidate that looks good immediately can produce a bad sub-tree.

## Core Contribution
- **Methodology:** For each candidate, perform trial branching on its children (second level) before committing; score the candidate using the deeper trial results and the implications (bound changes) they generate.
- **Assumptions:** Cheap trial LP solves; machinery to record and undo implied bound changes; candidate screening upstream.
- **Benchmarks/datasets:** MIP test problems of the era (not itemized in list).
- **Metrics:** Node counts vs. strong/reliability branching; total time including probe overhead.
- **Key results:** Deeper lookahead can cut the tree further than one-level strong branching at the cost of many more trial LP solves — the accuracy/cost frontier of branching.

## Engineering-Relevant Knowledge
**Algorithms:** Two-level lookahead branching; implication harvesting from trial solves.
**Techniques:** Record implied bound changes during trial solves (like probing/presolve implications) and credit them to the candidate; undo cleanly afterwards.
**Implementation details:** Our `src/milp/strong_branching.cpp` does one-level trials; adding level 2 multiplies probe LP solves — budget must be reported (probe count per node). Deep trials need reliable undo of bound changes, a correctness hazard.
**Equations/rules:** score(candidate) = f(child LP bounds at depth 2, implications found) rather than f(child LP bounds at depth 1).
**Limitations/failure cases:** Cost grows exponentially with depth (hence depth 2 only); sensitive to LP solver robustness on degenerate trial LPs.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** An upgrade to existing strong branching rather than a new subsystem, but expensive; adopt only if reliability branching leaves node counts high (R5, R20).

## Evidence → Engineering Decision
- *Finding:* one-level trials may mis-rank candidates → *PS requirement:* R5 → *Component:* src/milp/strong_branching.cpp → *Metric:* nodes vs. probe LP solves

## Related Papers
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Linderoth-2000-Impact-Branch-Bound]]
- [[Berthold-2006-Hybrid-Branching]]
- [[Driebeek-1966-Algorithm-Assignment-Problem]]

## Uses
- [[Strong Branching]] [[Pseudo-Cost Branching]] [[Branch and Bound]] [[Warm Start]]
