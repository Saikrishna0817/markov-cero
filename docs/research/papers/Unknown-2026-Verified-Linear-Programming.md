---
type: paper
title: "Verified Linear Programming via Tolerance-Aware Precision Boosting"
authors: "(author not stated in list)"
year: 2026
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ✦
status: standard
tags: [paper, numerics]
---

# Verified Linear Programming via Tolerance-Aware Precision Boosting

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> States the conditions under which precision-boosted (e.g. float→double→quad) simplex provably reproduces exact-arithmetic pivots — verified LP without full exact arithmetic.

## Metadata
| Field | Value |
|---|---|
| Authors | **not stated in reference list** — slug uses `Unknown` (see manifest) |
| Year | 2026 (from arXiv id 2609.11721) |
| Venue | (not stated in list; arXiv preprint) |
| DOI/URL | arXiv:2609.11721 (link in list) |

## Problem Addressed
Full exact arithmetic (#155) is slow; ordinary floating point is unverified. This work asks for the middle: at what precision does a floating-point simplex provably take the *same pivots* as the exact one, so its answer can be certified?

## Core Contribution
- **Methodology:** Relate tolerance choices to achievable precision: if all decision quantities (reduced costs, ratios) are resolved beyond a bound derived from κ and ε, boosted-precision simplex is pivot-equivalent to exact simplex, hence its result is verified.
- **Assumptions:** Ability to re-run at higher precision; condition estimates available; tolerance/precision pairs chosen consistently.
- **Benchmarks/datasets:** LPs requiring verification (not itemized in list).
- **Metrics:** Number of precision boosts; verification success; cost vs. exact.
- **Key results:** Gives checkable criteria for "verified without exact arithmetic" — the practical certification route for a from-scratch solver.

## Engineering-Relevant Knowledge
**Algorithms:** Tolerance-aware precision escalation in simplex; pivot-equivalence verification.
**Techniques:** Precision ladder (double → long double / __float128 → exact); record the precision at which each pivot became unambiguous.
**Implementation details:** Highly compatible with C++20 (`long double`, `__float128`) and with our verifier-first design: run LP at double, re-verify basis at higher precision in `src/verify/`. This is likely the cheapest credible R17 mechanism. (Recent preprint — verify claims before citing in the final report.)
**Equations/rules:** Precision p sufficient when margin(pivot quantities) > bound(κ, ε_p) for all pivotal comparisons.
**Limitations/failure cases:** κ too large ⇒ even quad precision insufficient; re-run cost; recent/unpeer-reviewed at time of listing.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Aligns exactly with R17 ("demonstrate numerical robustness") and with our existing independent verifiers — a certification protocol, not a new solver.

## Evidence → Engineering Decision
- *Finding:* verifiers exist but no precision-escalation protocol → *PS requirement:* R9, R17 → *Component:* src/verify/, src/lp/dual/dual_simplex.cpp → *Metric:* verified-solve rate, [[Numerical Error]]

## Related Papers
- [[Gleixner-2015-Iterative-Refinement-Linear]]
- [[Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Gartner-1999-Exact-Arithmetic-Low]]
- [[Azulay-0000-Revised-Simplex-Method]]

## Uses
- [[Iterative Refinement]] [[Numerical Stability]] [[Ill-Conditioning]] [[KKT Residual]]
