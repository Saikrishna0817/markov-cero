---
type: paper
title: "An Additive Algorithm for Solving LPs with Zero-One Variables"
authors: "Balas"
year: 1965
venue: "Operations Research"
doi: "(unverified)"
domain: [milp]
priority: ★
status: deep
tags: [paper, milp]
---

# An Additive Algorithm for Solving LPs with Zero-One Variables

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Implicit enumeration without full LP solves per node — additive bound updates over a 0-1 structure.

## Metadata
| Field | Value |
|---|---|
| Authors | Balas |
| Year | 1965 |
| Venue | Operations Research |
| DOI/URL | (unverified) |

## Problem Addressed
Solving an LP relaxation at every node was expensive in 1965. For 0-1 problems Balas showed bounds can be *updated additively* when a variable is fixed: fixing a variable changes the objective bound by its reduced-cost contribution, so nodes are priced without re-optimizing.

## Core Contribution
- **Methodology:** Enumerate implicitly over the 0-1 cube; partial assignments define a bound obtained by adding contributions of fixed variables (cost differences); variables are fixed in an order that maximizes bound growth; infeasible/over-bound partial assignments are abandoned.
- **Assumptions:** 0-1 variables; additive (linear) objective/constraints; valid lower bound from relaxed variables.
- **Benchmarks/datasets:** Zero-one test problems of the era.
- **Metrics:** Nodes visited; bound per node; time vs. enumeration.
- **Key results:** Implicit enumeration solves instances exhaustive search cannot (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Additive implicit enumeration (ancestor of depth-first 0-1 search and branching-variable scoring).
**Techniques:** Partial-assignment bound accumulation; early abandonment.
**Implementation details:** Not used directly in markov-cero (we re-solve node LPs in `src/milp/node_lp.cpp`), but the *idea* — cheap bound estimation before a full LP — survives in pseudo-cost branching and diving heuristics (`src/milp/branch_selector.cpp`, `src/milp/heuristics.cpp`).
**Limitations/failure cases:** Additive bounds ignore interactions that LP would capture → weaker pruning ([[Weak Relaxation]]); restricted to 0-1 structure.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Explains the lineage of cheap bound estimation used by diving/pseudo-costs; R5's branch-and-bound requirement is better served by LP relaxations for general integers.

## Evidence → Engineering Decision
- *Finding:* Cheap bound estimates reduce LP solves per node → *PS requirement:* R5, R20 → *Component:* src/milp/heuristics.cpp (diving with tentative bound updates) → *Metric:* LP iterations per node; time to first incumbent.

## Related Papers
- [[Land-1960-Automatic-Method-Solving]]
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Achterberg-2005-General-Mixed-Integer]]

## Uses
- [[Branch and Bound]]
- [[LP Relaxation]]
- [[Weak Relaxation]]
