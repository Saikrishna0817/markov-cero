---
type: paper
title: "Cover Inequalities for Mixed-Integer Programs"
authors: "Atamtürk"
year: 2003
venue: "(not stated in list)"
doi: "(unverified)"
domain: [cuts]
priority: ○
status: standard
tags: [paper, cuts]
---

# Cover Inequalities for Mixed-Integer Programs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Modern cover generation beyond the basic 0/1 case: covers with mixed-integer variables, multi-row covers and better separation.

## Metadata
| Field | Value |
|---|---|
| Authors | Atamtürk |
| Year | 2003 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Balas–Zemel covers (#100) assume pure 0/1 rows, but industrial models mix binaries with general integers and continuous quantities (blending, lot-sizing), where the classic cover is either invalid or leaves strength unused.

## Core Contribution
- **Methodology:** Extends cover inequalities to mixed-integer rows (bounds on general integers), adds multi-row/aggregated covers, and gives separation heuristics at fractional points.
- **Assumptions:** Bounded variables; at least one integer member; row or small row-set relaxation.
- **Benchmarks/datasets:** Mixed-integer test problems of the era (not itemized in list).
- **Metrics:** Gap closed per cut, nodes, solve rate.
- **Key results:** Covers remain among the most effective families when extended to mixed rows — the standard "next cut family" recommendation after GMI/MIR.

## Engineering-Relevant Knowledge
**Algorithms:** Minimal/heavy covers for mixed rows; separation at incumbent or fractional solution; cover lifting.
**Techniques:** Aggregation of rows before cover extraction (pairs with #102); coefficient reduction before cover generation (presolve).
**Implementation details:** Would extend `src/milp/mir.cpp`'s row processing; the same fractional point we already compute drives separation — no new LP work.
**Equations/rules:** Cover C with Σ_{j∈C} a_j > b → Σ_{j∈C} ⌊x_j⌋ ≤ |C| − 1 for integer members; mixed rows need bound-aware forms.
**Limitations/failure cases:** Separation cost grows with row size; weak if applied only at root (same failure mode as our current cuts).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Direct upgrade path for our missing cover capability and relevant to MIPLIB/blend models in R19; ranked below the theory papers but above background for cut work.

## Evidence → Engineering Decision
- *Finding:* no cover/clique cuts in tree (audit: "needs cover/clique/odds") → *PS requirement:* R5, R19 → *Component:* src/milp/ (new separator), src/milp/cut_pool.cpp → *Metric:* root gap closure on binary instances ([[Cut Efficiency]])

## Related Papers
- [[Balas-1980-Cuts-Fixed-Rank]]
- [[Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Chung-2015-Computational-Study-Cutting]]

## Uses
- [[Cut Validity]] [[Mixed Integer Rounding Cut]] [[Weak Relaxation]] [[LP Relaxation]]
