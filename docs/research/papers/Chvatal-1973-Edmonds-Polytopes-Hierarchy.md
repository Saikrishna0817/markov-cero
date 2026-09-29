---
type: paper
title: "Edmonds Polytopes and a Hierarchy of Combinatorial Problems"
authors: "Chvátal"
year: 1973
venue: "Discrete Math."
doi: "(unverified)"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# Edmonds Polytopes and a Hierarchy of Combinatorial Problems

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Defines Chvátal–Gomory cuts and Chvátal rank: a hierarchy of tightening that provably reaches the integer hull in finitely many rounds.

## Metadata
| Field | Value |
|---|---|
| Authors | Chvátal |
| Year | 1973 |
| Venue | Discrete Mathematics (list: "Discrete Math.") |
| DOI/URL | (unverified) |

## Problem Addressed
Different combinatorial polytopes (matching, TSP, knapsack) seemed unrelated; there was no way to say how strong a formulation is or how many rounds of inequality-derivation are needed to describe all integer points.

## Core Contribution
- **Methodology:** From an LP description Ax ≤ b with integer b, take a valid inequality with integer coefficients, round its right-hand side down to obtain a new valid inequality (the Chvátal–Gomory cut); iterate. Chvátal rank = minimum number of rounds to reach the integer hull.
- **Assumptions:** Integer data (or scaled to integers); bounded polyhedron; cuts applied round-wise to all rows.
- **Benchmarks/datasets:** Edmonds' matching polytope, set covering, TSP substructures — theoretical examples.
- **Metrics:** Chvátal rank (rounds to integer hull); strength of formulations.
- **Key results:** Provides the theoretical yardstick for "how good is this relaxation", and proves finite (though potentially expensive) derivability of all valid inequalities.

## Engineering-Relevant Knowledge
**Algorithms:** CG-cut generation; rank as a formulation-strength measure.
**Techniques:** Rounding right-hand sides of aggregated integer-coefficient rows — the theory behind [[Mixed Integer Rounding Cut]] and aggregation (#102).
**Implementation details:** CG rank is a research metric, not a runtime feature; practical solvers apply one round (GMI/MIR) rather than iterating. Useful when judging whether our MILP relaxations are "weak" ([[Weak Relaxation]]) on refinery/planning models.
**Equations/rules:** Given Σ a_i x_i ≤ b with a_i, b integer (after scaling) and x ≥ 0, the CG cut is Σ ⌊a_i⌋ x_i ≤ ⌊b⌋.
**Limitations/failure cases:** Rank can be high (exponential in general); coefficient blow-up when data is scaled to integers; not directly computable at scale.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** It explains *why* one round of MIR helps and gives vocabulary for reporting relaxation strength — required reading for the numerical-robustness story (R13/R17), not a component we implement.

## Evidence → Engineering Decision
- *Finding:* rank/weakness of relaxation explains long solve times on hard MIPs → *PS requirement:* R13, R20 → *Component:* src/milp/mir.cpp, src/presolve/presolve.cpp → *Metric:* root gap ([[Relative Optimality Gap]], [[Weak Relaxation]])

## Related Papers
- [[Gomory-1958-Outline-Algorithm-Integer]]
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Richard-2010-Group-Approach-Cutting]]
- [[Balas-1993-Lift-Project-Cutting]]

## Uses
- [[Weak Relaxation]] [[Mixed Integer Rounding Cut]] [[LP Relaxation]] [[Cut Validity]]
