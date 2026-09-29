---
type: paper
title: "Cuts of Fixed Rank in Zero-One Matrices"
authors: "Balas & Zemel"
year: 1980
venue: "Math. Prog."
doi: "(unverified)"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# Cuts of Fixed Rank in Zero-One Matrices

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Knapsack cover cuts: inequalities of fixed Chvátal rank that are among the most effective cuts for binary packing/covering rows.

## Metadata
| Field | Value |
|---|---|
| Authors | Balas & Zemel |
| Year | 1980 |
| Venue | Mathematical Programming (list: "Math. Prog.") |
| DOI/URL | (unverified) |

## Problem Addressed
For 0/1 knapsack-style rows, generic rounding cuts are weak, and it was unknown whether one could generate strong inequalities cheaply and predict their strength (rank) in advance.

## Core Contribution
- **Methodology:** Define cuts of fixed Chvátal rank on 0/1 matrices and show knapsack-cover inequalities arise as rank-bounded families; give procedures to generate them from minimal covers of the row.
- **Assumptions:** 0/1 variables; a single knapsack row (or its relaxation); capacity right-hand side.
- **Benchmarks/datasets:** Combinatorial examples (packing/covering structures); no modern suite.
- **Metrics:** Rank of the generated cut; number of cuts per row; gap closed.
- **Key results:** Established covers as *the* practical cut family for binary packing, and gave a principled (rank) way to measure cut quality rather than eyeballing it.

## Engineering-Relevant Knowledge
**Algorithms:** Minimal-cover enumeration, cover inequality generation, cut lifting to reuse all row coefficients.
**Techniques:** Minimal cover → cover inequality → lifting (coefficient improvement using remaining capacity); root-node separation over fractional solutions.
**Implementation details:** Our solver has **no cover cut** — only GMI and MIR — so binary packing/blending rows in MIPLIB and refinery models are under-served. A cover separator would slot next to `src/milp/gomory.cpp`/`mir.cpp`.
**Equations/rules:** For row Σ_{j∈S} a_j x_j ≤ b: a cover C ⊆ S with Σ_{j∈C} a_j > b gives Σ_{j∈C} x_j ≤ |C| − 1.
**Limitations/failure cases:** Minimal-cover enumeration is exponential in general (needs pruning at the fractional point); lifted coefficients must be recomputed if bounds change (see #98).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Highest-value missing cut family for binary packing models; moderate implementation effort and it is explicitly the kind of "advanced cut" R5 expects beyond a bare GMI/MIR baseline.

## Evidence → Engineering Decision
- *Finding:* only GMI+MIR implemented; covers absent → *PS requirement:* R5, R20 → *Component:* src/milp/ (new cover separator), src/milp/cut_pool.cpp → *Metric:* root gap closure, nodes on binary MIPLIB instances ([[Cut Efficiency]])

## Related Papers
- [[Atamturk-2003-Cover-Inequalities-Mixed]]
- [[Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Padberg-2005-Classical-Cuts-Mixed]]
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy]]

## Uses
- [[Cut Validity]] [[LP Relaxation]] [[Weak Relaxation]] [[Cut Efficiency]]
