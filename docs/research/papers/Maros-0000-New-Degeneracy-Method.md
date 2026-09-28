---
type: paper
title: "A New Degeneracy Method and Steepest-Edge-Based Conditioning for LP"
authors: "Maros"
year: 0000
venue: "SIMAX"
doi: "10.1137/S1052623494277470"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# A New Degeneracy Method and Steepest-Edge-Based Conditioning for LP

> Recursive degeneracy resolution plus cheap basis-condition estimates derived from steepest-edge weights.

## Metadata
| Field | Value |
|---|---|
| Authors | Maros |
| Year | **not stated in list** — slug uses 0000 (DOI embeds "94", suggesting 1994 — unverified); see manifest |
| Venue | SIAM Journal on Matrix Analysis and Applications (list: "SIMAX") |
| DOI/URL | 10.1137/S1052623494277470 |

## Problem Addressed
Degeneracy makes pivots meaningless and basis conditioning unpredictable; separately, computing κ(A) exactly is expensive. Maros links the two: the steepest-edge machinery already in the simplex can report *both* degeneracy and conditioning cheaply.

## Core Contribution
- **Methodology:** (a) A recursive procedure to resolve degenerate vertices (chain of degenerate pivots treated as one unit); (b) condition estimates derived from steepest-edge norms, giving a cheap per-iteration indicator of basis quality.
- **Assumptions:** Steepest-edge (or approximate) edge norms maintained by the simplex; bounded LP.
- **Benchmarks/datasets:** Degenerate/ill-conditioned LPs (SIMAX paper).
- **Metrics:** Degenerate pivot chains; condition estimate accuracy vs. cost.
- **Key results:** Two diagnostics from one data structure — degeneracy handling and conditioning monitoring — at steepest-edge cost.

## Engineering-Relevant Knowledge
**Algorithms:** Recursive degeneracy resolution; steepest-edge-based condition estimation.
**Techniques:** Use edge norms as a conditioning proxy (cheaper than κ̂); detect degenerate chains and pivot through them as a block.
**Implementation details:** We have **no steepest-edge at all** (audit: Phase 8 deferred; Bland-only) — so this paper's benefits are gated on implementing edge norms first (`src/lp/dual/dual_simplex.cpp`). Worth recording as the payoff case for that deferred phase.
**Equations/rules:** Basis condition proxy from accumulated edge-norm ratios (details in paper); degeneracy test: zero-length step with positive reduced costs within tolerance.
**Limitations/failure cases:** Steepest-edge adds O(n) work per pricing iteration (or approximation error); requires the tolerance policy to be sound first.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Relevant once steepest-edge lands (deferred Phase 8); today it documents *why* steepest-edge matters for R13/R17 — cheap conditioning visibility.

## Evidence → Engineering Decision
- *Finding:* no steepest-edge, no conditioning indicator → *PS requirement:* R13, R17 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* condition estimate over iterations, [[Ill-Conditioning]]

## Related Papers
- [[Charnes-1954-Optimality-Multi-Valuedness]]
- [[DeFarias-2019-Positive-Edge-Pricing]]
- [[Cline-1979-Estimate-Condition-Number]]
- [[Maros-1993-Practical-Anti-Degeneracy]]

## Uses
- [[Steepest Edge]] [[Degeneracy]] [[Ill-Conditioning]] [[Dual Simplex]]
