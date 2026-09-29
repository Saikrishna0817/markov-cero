---
type: paper
title: "On Numerical Stability of Simplex Algorithms"
authors: "Georg & Hettich, 1987/2007"
year: 1987
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# On Numerical Stability of Simplex Algorithms

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Argues Bartels–Golub updating is stable **iff** the tolerances are chosen from error estimates — stability is a tolerance policy, not a pivot rule.

## Metadata
| Field | Value |
|---|---|
| Authors | Georg & Hettich |
| Year | 1987 (list also gives 2007 — likely original + reprint/second version) |
| Venue | Optimization (journal, per DOI-link style in list); not stated explicitly |
| DOI/URL | (unverified; Taylor & Francis link in list) |

> **Possible duplicate with [[Ogryczak-1987-Numerical-Stability-Simplex]]:** two entries (#150, #151) share the same title and year with different authors. Verify whether these are one paper mis-attributed — see manifest.

## Problem Addressed
Published rounding-error analyses of simplex updating reach opposite conclusions; the practical question is which combination of update scheme *and* tolerance rule actually keeps results accurate.

## Core Contribution
- **Methodology:** Analyze the error introduced by Bartels–Golub basis updating and show that with tolerance rules derived from the accumulated error estimate the algorithm behaves in a backward-stable sense; naive fixed tolerances break the guarantee.
- **Assumptions:** Error bounds on updates tracked; tolerances adjusted accordingly.
- **Benchmarks/datasets:** LP bases / linear systems.
- **Metrics:** Error of final solution vs. tolerance rule; feasibility tolerance drift.
- **Key results:** Tolerance policy decides stability — fixed tolerances (the common choice) are the weak link.

## Engineering-Relevant Knowledge
**Algorithms:** Bartels–Golub updating with error-estimating tolerances.
**Techniques:** Derive feasibility/optimality tolerances from accumulated rounding error rather than constants like 1e-6/1e-7.
**Implementation details:** Our tolerances are currently fixed constants in the LP code — this paper says that is precisely the instability source. Combine with #142 (growth monitoring) and #144 (κ̂).
**Equations/rules:** Tolerance ← max(absolute floor, κ̂ · ε · scale factor) style adaptive rule.
**Limitations/failure cases:** Error estimates are themselves approximations; adaptive tolerances make behavior instance-dependent (harder to reproduce benchmarks).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Concrete, implementable improvement (tolerance policy) with direct bearing on R9/R13; also explains failures on degenerate instances.

## Evidence → Engineering Decision
- *Finding:* fixed LP tolerances, Bland-only anti-cycling → *PS requirement:* R9, R13 → *Component:* src/lp/dual/dual_simplex.cpp, src/lp/reference/revised_simplex.cpp → *Metric:* [[Numerical Error]], feasibility violation of returned solutions

## Related Papers
- [[Ogryczak-1987-Numerical-Stability-Simplex]]
- [[Gill-1974-Methods-Modifying-Matrix]]
- [[Cline-1979-Estimate-Condition-Number]]
- [[Bartels-1968-Numerical-Investigation-Simplex]]

## Uses
- [[Numerical Stability]] [[Basis]] [[Harris Ratio Test]] [[Ill-Conditioning]]
