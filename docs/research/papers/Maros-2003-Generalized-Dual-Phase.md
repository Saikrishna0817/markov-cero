---
type: paper
title: "A Generalized Dual Phase-2 Simplex Algorithm (cross-ref #40)"
authors: "Maros"
year: 2003
venue: "EJOR"
doi: "(unverified)"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# A Generalized Dual Phase-2 Simplex Algorithm (cross-ref #40)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Bound-flipping (BFRT) dual pivots that walk past a degenerate dual vertex in one iteration instead of stalling on it.

> **Collision note:** list entry **#162** explicitly cross-references **#40** (Module 3, same paper/title). The canonical slug `Maros-2003-Generalized-Dual-Phase` is occupied by the Module 3 note. This file keeps Module 11's copy at a disambiguated slug (`Phase-2` kept as a compound) — merge candidate, see manifest.

## Metadata
| Field | Value |
|---|---|
| Authors | Maros |
| Year | 2003 |
| Venue | European Journal of Operational Research (list: "EJOR") |
| DOI/URL | (unverified; Imperial College report DTR01-2 PDF in list) |

## Problem Addressed
The dual simplex stalls when the leaving-variable ratio ties (dual degeneracy): the ratio test picks a candidate that produces a zero-length step. A bound-flipping ratio test (BFRT) lets the iteration flip the *entering* variable's bound and keep moving instead of pivoting in place — critical because our dual simplex solves every MIP node.

## Core Contribution
- **Methodology:** Generalize the dual phase-2 ratio test so that when the minimum ratio is attained by several candidates (or immediately exhausts a bound), the algorithm flips that bound and continues in the same direction — multi-pivot, bound-flipping iterations with monotone objective.
- **Assumptions:** Bounded variables (bounds are the pivoting resource); phase-2 dual feasibility maintained; tie detection tolerance-consistent.
- **Benchmarks/datasets:** LPs with heavy bound activity / degenerate dual vertices (qualitative in list).
- **Metrics:** Iterations per LP solve; degenerate pivots avoided; time.
- **Key results:** Fewer, better pivots where the classical rule stalls — directly applicable to degenerate node relaxations (R13).

## Engineering-Relevant Knowledge
**Algorithms:** Generalized dual phase-2 simplex; BFRT (bound-flipping ratio test).
**Techniques:** Flip bounds instead of pivoting on ties; combine with [[Harris Ratio Test]] tolerances; track flipped bounds for clean undo (needed for trial solves in [[Strong Branching]]).
**Implementation details:** `src/lp/dual/dual_simplex.cpp` (LP engine for `src/milp/node_lp.cpp`). Audit flags dual-simplex internals (pricing, tolerances, BFRT) as unverified — check for bound-flip handling before claiming degeneracy robustness.
**Equations/rules:** λ = min_j (bound slack_j / reduced-cost ratio_j); if several j attain λ or the chosen j exhausts its bound, flip that bound and recompute rather than accepting a zero-length pivot; objective decrease accumulates across flips.
**Limitations/failure cases:** Bound-bookkeeping errors cause cycling or wrong bounds; multi-pivot steps complicate anti-cycling and verification; benefit is model-dependent — measure, don't assume.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R13's degenerate models produce exactly the bound-flipping stalls this addresses, and the fix lands in the component used at every node of the tree.

## Evidence → Engineering Decision
- *Finding:* dual simplex at every MIP node; BFRT/tie-handling unverified → *PS requirement:* R13, R17 → *Component:* src/lp/dual/dual_simplex.cpp, src/milp/node_lp.cpp → *Metric:* iterations per node LP, [[Degeneracy]] pivot counts

## Related Papers
- [[DeFarias-2019-Positive-Edge-Pricing]]
- [[Maros-1993-Practical-Anti-Degeneracy]]
- [[Charnes-1954-Optimality-Multi-Valuedness]]
- [[Gill-1989-Practical-Anti-Cycling]]

## Uses
- [[Dual Simplex]] [[Degeneracy]] [[Harris Ratio Test]] [[Warm Start]]
