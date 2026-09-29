---
type: paper
title: "Feasibility and Redundancy in Linear Programming"
authors: "Chinneck"
year: 1992
venue: "IJOC"
doi: "(unverified)"
domain: [presolve]
priority: ○
status: standard
tags: [paper, presolve]
---
# Feasibility and Redundancy in Linear Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Heuristics for detecting infeasibility and redundant rows/columns in LP models.
## Metadata
| Field | Value |
|---|---|
| Authors | Chinneck |
| Year | 1992 |
| Venue | IJOC |
| DOI/URL | (unverified) |
## Problem Addressed
Industrial models frequently contain redundant constraints or rows that make the system infeasible as written; solvers waste effort or report misleading failures. Redundancy/infeasibility should be diagnosed before the simplex/IPM run.
## Core Contribution
- **Methodology:** Heuristic tests to identify rows/columns implied by the rest of the system (redundant rows, forced variables, infeasibility certificates); order tests by cost/benefit.
- **Assumptions:** LP in general form; approximate rank/bound tests acceptable for *detection* (each removal still verified).
- **Benchmarks/datasets:** Models with known injected redundancies (paper's set).
- **Metrics:** Detected redundant rows; false-positive rate; time spent.
- **Key results:** Heuristics catch most redundancy cheaply (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Redundancy/infeasibility screening.
**Techniques:** Bound-based row implication tests; Farkas-style infeasibility evidence (ties to our exit code 1 certificates).
**Implementation details:** Complements our verifier stack: `src/verify/primal_verifier.cpp` detects violations, but no pre-solve redundancy screening exists.
**Limitations/failure cases:** Detection itself can be as hard as solving; heuristics can miss subtle redundancy ([[Ill-Conditioning]] near-rank-deficient rows).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R13 (degenerate/ill-conditioned models) and R17 (reliable convergence): removing redundancy early prevents rank-deficient factorizations and misleading infeasibility reports.
## Evidence → Engineering Decision
- *Finding:* Redundant rows cause singular bases and false infeasibility risk → *PS requirement:* R13, R17 → *Component:* src/presolve/presolve.cpp (redundancy screen) → *Metric:* exit-7/exit-1 misclassification rate.
## Related Papers
- [[Brearley-1975-Analysis-Mathematical-Programming]]
- [[Andersen-1995-Presolving-Linear-Programming]]
## Uses
- [[Presolve]]
- [[Degeneracy]]
