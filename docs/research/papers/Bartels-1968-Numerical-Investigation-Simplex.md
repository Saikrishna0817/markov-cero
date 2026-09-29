---
type: paper
title: "A Numerical Investigation of the Simplex Method"
authors: "Bartels"
year: 1968
venue: "PhD thesis / STAN-CS-68-104, Stanford"
doi: "10.21236/AD0673010"
domain: [numerics]
priority: ✦
status: standard
tags: [paper, numerics]
---

# A Numerical Investigation of the Simplex Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Demonstrates that naive basis-inverse updating is numerically unstable — the empirical failure that motivated stable LU updating and refactorization policies.

## Metadata
| Field | Value |
|---|---|
| Authors | Bartels |
| Year | 1968 |
| Venue | PhD thesis / STAN-CS-68-104, Stanford |
| DOI/URL | 10.21236/AD0673010 |

## Problem Addressed
Early simplex codes computed and updated an explicit basis inverse; nobody had measured how rounding error accumulates over a long pivot sequence or whether results remain meaningful.

## Core Contribution
- **Methodology:** Instrument simplex runs and measure rounding-error growth for different basis representations/updating schemes; compare computed results against exact or high-precision references.
- **Assumptions:** Available LP test problems; ability to compute reference solutions accurately.
- **Benchmarks/datasets:** LPs of the late 1960s (predecessor of NETLIB sets).
- **Metrics:** Error of objective/residual over pivot count; growth by scheme.
- **Key results:** Showed naive inverse updating degrades badly — directly motivating Bartels–Golub (1969) factorized updating and the update-vs-refactorize policy of #142.

## Engineering-Relevant Knowledge
**Algorithms:** Error measurement across a simplex run; comparison of basis representations.
**Techniques:** Track residuals/objective error per iteration as a regression test in CI for our LP engine.
**Implementation details:** Cheap to adopt: log ‖Ax−b‖ and duality gap per N pivots in `src/lp/reference/revised_simplex.cpp` as a test signal (pairs with `tests/` numerical suites).
**Equations/rules:** none beyond residual measurement.
**Limitations/failure cases:** 1960s arithmetic and problems; conclusions about *which* scheme wins are historical — the measurement discipline is not.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Historical evidence that motivates our basis-update design (R6/R9) and gives the audit a citation for why we must measure, not assume, stability.

## Evidence → Engineering Decision
- *Finding:* no per-iteration error tracking in our LP tests → *PS requirement:* R9, R17 → *Component:* src/lp/reference/revised_simplex.cpp, tests/ → *Metric:* [[Numerical Error]] growth per pivot

## Related Papers
- [[Gill-1974-Methods-Modifying-Matrix]]
- [[Ogryczak-1987-Numerical-Stability-Simplex]]
- [[Georg-1987-Numerical-Stability-Simplex]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic]]

## Uses
- [[Numerical Stability]] [[Basis]] [[Revised Simplex]] [[Ill-Conditioning]]
