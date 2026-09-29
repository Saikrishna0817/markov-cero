---
type: paper
title: "Positive-Edge Pricing Rule for the Dual Simplex"
authors: "de Farias et al."
year: 2019
venue: "EJOR"
doi: "(unverified)"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# Positive-Edge Pricing Rule for the Dual Simplex

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> A practical anti-degeneracy pricing rule for the dual simplex: prefer edges whose reduced-cost ratios keep the iterate strictly improving.

## Metadata
| Field | Value |
|---|---|
| Authors | de Farias et al. |
| Year | 2019 |
| Venue | European Journal of Operational Research (list: "EJOR") |
| DOI/URL | (unverified; HAL PDF in list) |

## Problem Addressed
Dual simplex on degenerate LPs (the norm at MIP nodes) chooses among many zero-reduced-cost leaving candidates, takes a pivot that improves nothing, and stalls. Standard pricing rules are blind to this.

## Core Contribution
- **Methodology:** Rank leaving candidates by a "positive edge" criterion that favors pivots producing strict improvement and avoids the degenerate pivot cycles reported on network/industrial LPs; compare against Dantzig/Harris rules on degenerate relaxations.
- **Assumptions:** Dual simplex with reduced-cost ratios available; degenerate vertices common.
- **Benchmarks/datasets:** Degenerate LPs, network LPs, MIP-node relaxations.
- **Metrics:** Degenerate pivots, iterations to optimal, time per LP.
- **Key results:** Reduces degenerate pivoting at modest cost — a targeted answer to degeneracy that does *not* require perturbing the problem.

## Engineering-Relevant Knowledge
**Algorithms:** Positive-edge pricing for [[Dual Simplex]]; anti-degeneracy pivot selection.
**Techniques:** Tie-breaking by edge positivity; combining with [[Harris Ratio Test]] tolerances.
**Implementation details:** Pricing lives in `src/lp/dual/dual_simplex.cpp` (used at every MIP node) — the single cheapest place to attack R13's degeneracy requirement. Our current pricing/tolerance status is an open audit item (TB-xx), so measure degenerate-pivot counts before/after.
**Equations/rules:** Choose leaving row minimizing positive-edge score among candidates with minimum reduced-cost ratio (details in paper); must not violate dual feasibility.
**Limitations/failure cases:** Extra bookkeeping per pricing loop; benefit only on degenerate LPs; must be validated with tolerance-consistent ratio tests (ties to Harris).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Directly targets "highly degenerate models" (R13) inside the engine we use at every node, with a small, testable change.

## Evidence → Engineering Decision
- *Finding:* dual simplex at every node; degeneracy handling unverified → *PS requirement:* R13, R17 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* degenerate pivots per LP, [[Degeneracy]] solve rate

## Related Papers
- [[Charnes-1954-Optimality-Multi-Valuedness]]
- [[Maros-1993-Practical-Anti-Degeneracy]]
- [[Gill-1989-Practical-Anti-Cycling]]
- [[Maros-0000-New-Degeneracy-Method]]

## Uses
- [[Dual Simplex]] [[Degeneracy]] [[Harris Ratio Test]] [[Reduced Cost]]
