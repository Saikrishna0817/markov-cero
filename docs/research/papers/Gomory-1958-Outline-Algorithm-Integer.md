---
type: paper
title: "Outline of an Algorithm for Integer Solutions to Linear Programs"
authors: "Gomory"
year: 1958
venue: "Bull. AMS"
doi: "(unverified)"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# Outline of an Algorithm for Integer Solutions to Linear Programs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Origin of cutting planes: derive an integer-invalid inequality from a fractional simplex tableau row and re-optimize.

## Metadata
| Field | Value |
|---|---|
| Authors | Gomory |
| Year | 1958 |
| Venue | Bulletin of the American Mathematical Society (list: "Bull. AMS") |
| DOI/URL | (unverified) |

## Problem Addressed
Integer programs could only be attacked by enumeration; there was no systematic way to tighten an LP relaxation while keeping every integer point. Gomory asked how to cut off a fractional vertex of the LP polytope without cutting off any integer solution.

## Core Contribution
- **Methodology:** Take the final (fractional) simplex tableau row, construct an inequality that excludes the current LP vertex but is valid for all integer points, append it, re-optimize; finite convergence proven for pure ILP with rational data.
- **Assumptions:** All variables integer, rational data, exact arithmetic — finite termination of the proof assumes exact pivots.
- **Benchmarks/datasets:** Small hand-worked examples; no benchmark suite existed in 1958.
- **Metrics:** Cuts required to reach an integer optimum; finite-termination proof.
- **Key results:** Establishes the entire cut paradigm; also the paper whose naive floating-point reimplementations later showed severe instability, motivating the mixed-integer and lifted variants.

## Engineering-Relevant Knowledge
**Algorithms:** Tableau-row cut generation; cut → re-optimize loop, which is exactly the separation phase of root-node [[Branch and Cut]].
**Techniques:** Fractional-part extraction from a basis row; adding rows without full refactorization (re-optimize by [[Dual Simplex]]).
**Implementation details:** Root separation lives in `src/milp/gomory.cpp`; audit evidence says root-only cuts yield **0.0% node reduction**, so placement (root vs. tree) and cut quality dominate the historical recipe.
**Equations/rules:** Row x_0 = b̄ − Σ ā_j x_j; cut coefficients are derived from the fractional parts of ā_j and b̄. Exact published forms differ by variant — verify against `src/milp/gomory.cpp` before trusting a formula.
**Limitations/failure cases:** All-integer formulation only (does not directly cover mixed rows); notoriously unstable in naive double precision; superseded for MIP by GMI/split/clift cuts.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** The 1958 all-integer cut does not match our mixed rows; the GMI variant (#94) is what we implement. The paper still defines [[Cut Validity]] and the separation loop we must schedule (root-only today).

## Evidence → Engineering Decision
- *Finding:* root-only cuts give 0.0% node reduction (`evidence/benchmarks/phase4.json`) → *PS requirement:* R5 → *Component:* src/milp/gomory.cpp → *Metric:* node-count reduction, root gap closure ([[Cut Efficiency]])
- *Finding:* historical floating-point instability of tableau cuts → *PS requirement:* R9, R13 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* [[Numerical Error]]

## Related Papers
- [[Gomory-1963-All-Integer-Programming]]
- [[Balas-1996-Gomory-Cuts-Revisited]]
- [[Padberg-2005-Classical-Cuts-Mixed]]
- [[Bartels-1968-Numerical-Investigation-Simplex]]

## Uses
- [[Gomory Mixed Integer Cut]] [[Cut Validity]] [[LP Relaxation]] [[Branch and Cut]]
