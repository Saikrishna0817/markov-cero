---
type: paper
title: "The Mixed Integer Rounding Family of Inequalities / Computational Study of L&P Cuts"
authors: "Marchand & Wolsey; Cornuéjols & Tuintra"
year: 1996
venue: "(not stated in list)"
doi: "(unverified)"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# The Mixed Integer Rounding Family of Inequalities / Computational Study of L&P Cuts

> MIR inequalities generalize GMI to arbitrary rows and remain the workhorse cut family of modern MIP solvers.

## Metadata
| Field | Value |
|---|---|
| Authors | Marchand & Wolsey (primary); Cornuéjols & Tuintra (companion study) |
| Year | 1996 (Marchand & Wolsey); Cornuéjols & Tuintra year not given in list |
| Venue | (not stated in list — split entry: MIR theory + L&P computational study) |
| DOI/URL | (unverified) |

## Problem Addressed
GMI cuts (#94) need a row whose coefficients are already tableau-like and handle only one integer variable cleanly. Practitioners needed a cut that works on *any* mixed row — with continuous slack variables — and a computational verdict on how lift-and-project compares.

## Core Contribution
- **Methodology:** Mixed-integer rounding: split a row on the fractional part of its right-hand side, take the convex combination of the two resulting half-space descriptions, and rescale coefficients so the inequality stays valid for all integer values of the designated integer variable; the companion study evaluates L&P cuts computationally.
- **Assumptions:** Row with one integer variable (or aggregations producing one), x ≥ 0 after bound-shifting, bounded relaxation.
- **Benchmarks/datasets:** MIR: structured mixed rows; L&P study: MIP test sets of the mid-1990s (exact instances not given in list).
- **Metrics:** Gap closure per cut, nodes/time vs. no cuts.
- **Key results:** MIR subsumes GMI and generalizes to aggregations with continuous variables — this is why `src/milp/mir.cpp` exists alongside `gomory.cpp`.

## Engineering-Relevant Knowledge
**Algorithms:** [[Mixed Integer Rounding Cut]] generation; aggregation-then-MIR (ties to #102).
**Techniques:** Choosing the "designated" integer variable; coefficient rounding/tightening; cut selection among a family.
**Implementation details:** Our MIR implementation is root-only; both MIR and GMI are present, which matches this paper's message (one family is not enough). Bound-shifting before rounding must use the *current* node bounds if cuts ever move below root.
**Equations/rules:** For row Σ a_j x_j ≥ b with y integer: f = b − ⌊b⌋; coefficients below f are kept/rescaled to give an inequality of the form Σ_{a_j ≤ f} a_j x_j + Σ_{a_j > f} ((1 − a_j + f)/(1 − f)) x_j ≥ f (mirror form when f > 0.5). *(Verify sign convention against `src/milp/mir.cpp`.)*
**Limitations/failure cases:** Weak on tightly-coupled knapsack rows (use covers #100/#101); weak if aggregation picks a poor integer variable.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** This is the theory behind the second cut family we ship; it tells us when MIR beats GMI and what to test when root gap closure stalls.

## Evidence → Engineering Decision
- *Finding:* we ship GMI + MIR but measure 0.0% node reduction → *PS requirement:* R5 → *Component:* src/milp/mir.cpp, src/milp/cut_pool.cpp → *Metric:* root gap closure ([[Cut Efficiency]])
- *Finding:* L&P strength varies by family → *PS requirement:* R16 → *Component:* src/milp/milp_solver.cpp → *Metric:* node counts vs. comparison solver ([[Relative Optimality Gap]])

## Related Papers
- [[Gomory-1963-All-Integer-Programming]]
- [[Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Padberg-2005-Classical-Cuts-Mixed]]
- [[Balas-1993-Lift-Project-Cutting]]

## Uses
- [[Mixed Integer Rounding Cut]] [[Gomory Mixed Integer Cut]] [[Cut Validity]] [[Cut Efficiency]]
