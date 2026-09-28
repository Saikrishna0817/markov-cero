---
type: paper
title: "Exact Algorithms for Linear Programming / Projective Test"
authors: "Steinrücken, 2019; Cook & Koch"
year: 2019
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# Exact Algorithms for Linear Programming / Projective Test

> Rational-arithmetic fallback: solve the LP exactly when floating point is untrustworthy, including a projective test for phase-one infeasibility.

## Metadata
| Field | Value |
|---|---|
| Authors | Steinrücken (2019); Cook & Koch (second, merged entry) |
| Year | 2019 (first) |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Some LPs cannot be resolved in double precision: κ·ε ≥ 1 means *no* floating-point answer is reliable, yet a correct answer exists in exact arithmetic. Solvers either fail or return an unchecked result.

## Core Contribution
- **Methodology:** Run simplex (or a simplex variant) over rational/integer arithmetic so every pivot decision and every infeasibility claim is exact; the "projective test" component addresses phase-one infeasibility detection in this framework.
- **Assumptions:** Data rational (MPS data is decimal ⇒ rational); growth of coefficient sizes must be controlled (reduction/gcd normalization).
- **Benchmarks/datasets:** Ill-conditioned LPs where floating-point solvers disagree.
- **Metrics:** Exact solve success; bit-length growth; time overhead vs. double.
- **Key results:** Provides the last-resort rung of the robustness ladder: double → refinement → exact. When exact and floating-point disagree, exact is right by definition.

## Engineering-Relevant Knowledge
**Algorithms:** Exact simplex; rational reconstruction; projective infeasibility test.
**Techniques:** Fraction-free pivoting; coefficient growth control; use exact only when κ̂ indicates failure.
**Implementation details:** A full exact engine is a large project (R10 forbids importing libraries). Realistic slice: exact *verification* of basis inverses/residuals for certification in `src/verify/`, borrowing the projective test idea for Phase I infeasibility claims.
**Equations/rules:** exact pivot requires exact solve of B x = a; maintain integers with periodic gcd reduction.
**Limitations/failure cases:** Coefficient swell makes exact pivot cost explode; not suitable for the hot path or GPU work (R8).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** As a certification backstop it directly serves R17; as a solver path it is out of proportion to the project's timeline.

## Evidence → Engineering Decision
- *Finding:* no fallback when floating point fails → *PS requirement:* R13, R17 → *Component:* src/verify/ (exact residual check), src/lp/dual/dual_simplex.cpp → *Metric:* false-status rate on hard instances

## Related Papers
- [[Gleixner-2015-Iterative-Refinement-Linear]]
- [[Gartner-1999-Exact-Arithmetic-Low]]
- [[Unknown-2026-Verified-Linear-Programming]]
- [[Neumaier-2004-Safe-Bounds-Linear]]

## Uses
- [[Numerical Stability]] [[Ill-Conditioning]] [[Dual Simplex]] [[KKT Residual]]
