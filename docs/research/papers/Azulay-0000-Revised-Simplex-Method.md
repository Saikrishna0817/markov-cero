---
type: paper
title: "A Revised Simplex Method with Integer Q-Matrices"
authors: "Azulay & Pique"
year: 0000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ✦
status: standard
tags: [paper, numerics]
---

# A Revised Simplex Method with Integer Q-Matrices

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Exact/multiprecision revised simplex via an integer Q-matrix formulation — keeping arithmetic integral so pivots are exact by construction.

## Metadata
| Field | Value |
|---|---|
| Authors | Azulay & Pique |
| Year | **not stated in list** — slug uses 0000 (ACM DL DOI 10.1145/1989734.1989738; year not derivable safely); see manifest |
| Venue | (not stated in reference list; ACM Digital Library link) |
| DOI/URL | 10.1145/1989734.1989738 (link in list) |

## Problem Addressed
Exact simplex normally means big rationals; if the LP data is scaled to integers and the algorithm works with an integer basis-inverse-related matrix (Q), arithmetic stays integral and pivots are unambiguous.

## Core Contribution
- **Methodology:** Reformulate the revised simplex so the quantity updated (Q = integer transformation of the basis) remains integral — no division until display — enabling exact pivoting with multiprecision integers rather than rationals.
- **Assumptions:** Integer (or scaled-to-integer) data; multiprecision integer arithmetic available.
- **Benchmarks/datasets:** LPs with integer data (not itemized in list).
- **Metrics:** Exactness of pivots; integer growth; time vs. rational exact simplex.
- **Key results:** A concrete design for exact LP that avoids rational arithmetic's cost — one of the feasible exact-fallback architectures.

## Engineering-Relevant Knowledge
**Algorithms:** Integer Q-matrix revised simplex; exact pivot selection.
**Techniques:** Keep transformations integral; normalize with gcd to control growth.
**Implementation details:** Relevant only to the certification backstop (`src/verify/`) or a future exact mode; we have no multiprecision integer type in the LP path today. Note also that a from-scratch solver must implement its own big-integer handling (R10).
**Equations/rules:** Q updated by unimodular-like pivot operations; solution recovered by division only at the end.
**Limitations/failure cases:** Integer growth is the practical limit (same swell problem as #155); slower than floating point by orders of magnitude.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Documents one architecture for an exact fallback; not scheduled work — the certification path via refinement/verification is higher priority.

## Evidence → Engineering Decision
- *Finding:* no multiprecision/exact option exists → *PS requirement:* R17 → *Component:* src/verify/ (future exact mode) → *Metric:* certificate verification success rate

## Related Papers
- [[Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Gartner-1999-Exact-Arithmetic-Low]]
- [[Unknown-2026-Verified-Linear-Programming]]
- [[Gleixner-2015-Iterative-Refinement-Linear]]

## Uses
- [[Revised Simplex]] [[Numerical Stability]] [[Iterative Refinement]] [[Basis]]
