---
type: paper
title: "Condition Numbers, the Linear Programming Problem and Sensitivity Analysis"
authors: "Renegar"
year: 1994
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Condition Numbers, the Linear Programming Problem and Sensitivity Analysis

> Gives LP a rigorous definition of "ill-conditioned": relative data perturbations produce bounded relative solution changes iff κ is bounded — with matching sensitivity bounds.

## Metadata
| Field | Value |
|---|---|
| Authors | Renegar |
| Year | 1994 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
"Ill-conditioned LP" was used loosely — sometimes the matrix, sometimes the data, sometimes the solution. Without a definition, claims of robustness (like ours for R13/R17) are unfalsifiable.

## Core Contribution
- **Methodology:** Define condition measures for the feasible region/optimum (in terms of data, RHS and objective perturbations) and prove perturbation theorems: relative change in solution/optimality ≤ κ × relative change in data; connect to sensitivity analysis.
- **Assumptions:** Feasible bounded LP; perturbations measured in appropriate norms; near-optimal solutions considered (not just vertices).
- **Benchmarks/datasets:** Theory; no datasets.
- **Metrics:** κ of the LP instance; sensitivity of optimum/RHS.
- **Key results:** The canonical citation for why a solver can fail on *degenerate or weakly feasible* instances even when the matrix looks fine — basis conditioning ≠ LP conditioning.

## Engineering-Relevant Knowledge
**Algorithms:** Condition measures for LP; sensitivity bounds for objective/RHS changes.
**Techniques:** Report problem-level conditioning, not only matrix κ̂ (#144); use near-optimality conditions to explain why solutions drift.
**Implementation details:** Supports the *language* of our robustness report: define what we mean by ill-conditioned before we claim to handle it (R17). Matrix-level κ̂ in `src/linalg/sparse_basis.cpp` plus problem-level condition from Renegar gives a defensible metric.
**Equations/rules:** ‖Δx‖/‖x‖ ≤ κ · ‖ΔA‖/‖A‖ style perturbation bounds (κ defined for the LP, not just A).
**Limitations/failure cases:** Renegar's κ can be hard to compute exactly; theoretical bounds may be loose for practical reporting.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Supplies the definition our R17 demonstration must rest on; no code, but it prevents us from claiming robustness without saying what condition number we mean.

## Evidence → Engineering Decision
- *Finding:* "ill-conditioned" is undefined in our docs/evidence → *PS requirement:* R13, R17 → *Component:* docs/decisions/ADR-M0-03-numerical-policy.md, src/verify/ → *Metric:* [[Ill-Conditioning]] definition, [[KKT Residual]]

## Related Papers
- [[Cline-1979-Estimate-Condition-Number]]
- [[Chinneck-1987-Primal-Dual-Methods]]
- [[Wilkinson-1963-Rounding-Errors-Algebraic]]
- [[Gleixner-2015-Iterative-Refinement-Linear]]

## Uses
- [[Ill-Conditioning]] [[Degeneracy]] [[Duality Gap]] [[KKT Conditions]]
