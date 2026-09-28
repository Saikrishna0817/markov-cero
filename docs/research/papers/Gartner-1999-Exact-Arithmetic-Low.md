---
type: paper
title: "Exact Arithmetic at Low Cost — A Case Study in Linear Programming"
authors: "Gärtner"
year: 1999
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ✦
status: standard
tags: [paper, numerics]
---

# Exact Arithmetic at Low Cost — A Case Study in Linear Programming

> Hybrid exact/floating-point simplex: do the work in double, verify/repair only where it matters — exactness for a fraction of the cost when m ≪ n.

## Metadata
| Field | Value |
|---|---|
| Authors | Gärtner |
| Year | 1999 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified; Springer link in list) |

## Problem Addressed
Pure exact arithmetic is prohibitively slow; pure floating point is unreliable on hard instances. The case study asks how much exactness you actually need, and where.

## Core Contribution
- **Methodology:** Run ordinary floating-point simplex and escalate selectively — exact computation only for the decisive operations (pivot selection near ties, residual checks), exploiting m ≪ n structure so exact solves stay on narrow vectors.
- **Assumptions:** Problem structure with few constraints relative to variables; ability to interleave precision levels.
- **Benchmarks/datasets:** LP instances chosen for numerical difficulty.
- **Metrics:** Time vs. full exact and full floating-point; number of exact operations triggered.
- **Key results:** Demonstrates a practical middle path — most of the exact-solver benefit at near-floating-point cost on favorable shapes.

## Engineering-Relevant Knowledge
**Algorithms:** Mixed-precision simplex with selective exact escalation.
**Techniques:** Exact fallback only on detected trouble (near-ties, residual failure) — the escalation ladder idea (#146 refinement, then exact).
**Implementation details:** Shape-dependent (m ≪ n): dense-column/large-row industrial models differ from our Netlib set. Nearest practical use: exact *verification* in `src/verify/` rather than exact pivoting.
**Equations/rules:** Trigger: if |reduced cost| < estimated error bound, recompute that quantity exactly.
**Limitations/failure cases:** Loses its advantage when m ≈ n or coefficients swell; must not be used per-iteration on the GPU path.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Frames exact arithmetic as an escalation strategy rather than a solver replacement — affordable and consistent with R10 (from scratch, no libraries).

## Evidence → Engineering Decision
- *Finding:* no escalation ladder from float → exact → certificate → *PS requirement:* R13, R17 → *Component:* src/verify/, src/lp/dual/dual_simplex.cpp → *Metric:* fraction of solves requiring escalation, [[Numerical Error]]

## Related Papers
- [[Steinrucken-2019-Exact-Algorithms-Linear]]
- [[Unknown-2026-Verified-Linear-Programming]]
- [[Moler-1967-Rounding-Errors-Algebraic]]
- [[Azulay-0000-Revised-Simplex-Method]]

## Uses
- [[Numerical Stability]] [[Iterative Refinement]] [[Ill-Conditioning]] [[Revised Simplex]]
