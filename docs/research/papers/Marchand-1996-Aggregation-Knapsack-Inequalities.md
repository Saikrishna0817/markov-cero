---
type: paper
title: "Aggregation and Knapsack Inequalities"
authors: "Marchand & Wolsey"
year: 1996
venue: "OR Letters"
doi: "(unverified)"
domain: [cuts]
priority: ○
status: standard
tags: [paper, cuts]
---

# Aggregation and Knapsack Inequalities

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Combining several weak rows into one aggregated row before rounding produces much stronger knapsack-style cuts than any single row.

## Metadata
| Field | Value |
|---|---|
| Authors | Marchand & Wolsey |
| Year | 1996 |
| Venue | Operations Research Letters (list: "OR Letters") |
| DOI/URL | (unverified) |

## Problem Addressed
Single-row cuts (GMI/MIR) cannot see coupling constraints: individually each row is weak, but a weighted sum of rows can expose a violated knapsack inequality that no single row reveals.

## Core Contribution
- **Methodology:** Choose nonnegative multipliers to aggregate a subset of rows into one knapsack row, then apply cover/MIR cuts to the aggregate; give criteria for picking multipliers that maximize expected strength.
- **Assumptions:** Rows share integer structure (or one designated integer variable); nonnegative multipliers keep direction/validity.
- **Benchmarks/datasets:** Mixed-integer test problems (not itemized in list).
- **Metrics:** Gap closed per aggregated cut; solve time with/without aggregation.
- **Key results:** Aggregation is a cheap, high-leverage source of strength — and the same mechanism later reused for MIR (#99) and by cut pools that merge rows.

## Engineering-Relevant Knowledge
**Algorithms:** Row aggregation (multiplier selection), then cover/MIR separation on the aggregate.
**Techniques:** Weighted sum of constraint rows; discarding aggregates that are parallel/dominated by existing cuts.
**Implementation details:** We aggregate only implicitly via presolve (4 rules in `src/presolve/presolve.cpp`); a deliberate aggregation step before `src/milp/mir.cpp` would be new functionality.
**Equations/rules:** Aggregate row: Σ_i μ_i (row_i), μ_i ≥ 0 ⇒ Σ_j (Σ_i μ_i a_ij) x_j ≤ Σ_i μ_i b_i; then apply cover/MIR to coefficients.
**Limitations/failure cases:** Dense aggregates destroy sparsity and slow FTRAN solves; multiplier choice is heuristic — bad choices yield weaker cuts at higher cost.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Reuses machinery we have (row combinations in presolve + MIR) and addresses the weak-relaxation complaint (R13) without a new cut family implementation.

## Evidence → Engineering Decision
- *Finding:* only 4 presolve rules and single-row cuts → *PS requirement:* R5, R13 → *Component:* src/presolve/presolve.cpp, src/milp/mir.cpp → *Metric:* root gap, LP time per node ([[Weak Relaxation]])

## Related Papers
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Balas-1980-Cuts-Fixed-Rank]]
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy]]
- [[Atamturk-2003-Cover-Inequalities-Mixed]]

## Uses
- [[Mixed Integer Rounding Cut]] [[Cut Validity]] [[Weak Relaxation]] [[Cut Pooling]]
