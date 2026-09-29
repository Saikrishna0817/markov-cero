---
type: paper
title: "On Numerical Stability of Simplex Algorithms (rounding-error analysis)"
authors: "Ogryczak"
year: 1987
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# On Numerical Stability of Simplex Algorithms (rounding-error analysis)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Rigorous rounding-error analysis: well-behaved factor updates *plus* consistent tolerances ⇒ simplex stability; either one alone is insufficient.

## Metadata
| Field | Value |
|---|---|
| Authors | Ogryczak |
| Year | 1987 |
| Venue | (not stated in reference list; author PDF link in list) |
| DOI/URL | (unverified) |

## Problem Addressed
Claims that "the simplex is numerically unstable" rest on isolated bad examples; claims that "it is stable" ignore accumulated update error. A rigorous, general analysis of the floating-point simplex was missing.

## Core Contribution
- **Methodology:** Track rounding errors through basis factorization, updates and solves over many pivots; derive conditions (well-behaved updating + tolerance handling) under which computed iterates remain close to exact ones.
- **Assumptions:** Model of floating-point arithmetic; specific update scheme analyzed.
- **Benchmarks/datasets:** Analytical examples + LP bases.
- **Metrics:** Error growth per pivot; conditions for bounded error.
- **Key results:** Stability is achievable but conditional — supports the same conclusion as #150 from a different analytical angle: implement the update *and* the tolerance policy together.

## Engineering-Relevant Knowledge
**Algorithms:** Error analysis of simplex updating (Bartels–Golub family).
**Techniques:** Consistency between tolerances and accumulated error; periodic refactorization as an error reset.
**Implementation details:** Backs the same code inspection as #150: does `src/linalg/sparse_basis.cpp` reset error by refactorization on a schedule, and are tolerances tied to error? Refactorization interval is a hidden stability knob.
**Equations/rules:** Error bound per pivot sequence grows with number of updates since refactorization — motivates a refactorization interval parameter.
**Limitations/failure cases:** Analysis is scheme-specific; worst-case bounds may be pessimistic; same possible duplication concern as #150 (verify identity of entries).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Turns "we refactorize every N pivots" from a performance guess into a defensible numerical policy (R9).

## Evidence → Engineering Decision
- *Finding:* refactorization/update error policy unrecorded → *PS requirement:* R9, R13 → *Component:* src/linalg/sparse_basis.cpp → *Metric:* [[Numerical Error]] vs. pivots since refactorization

## Related Papers
- [[Georg-1987-Numerical-Stability-Simplex]]
- [[Bartels-1968-Numerical-Investigation-Simplex]]
- [[Gill-1974-Methods-Modifying-Matrix]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic]]

## Uses
- [[Numerical Stability]] [[Basis]] [[Iterative Refinement]] [[Ill-Conditioning]]
