---
type: paper
title: "To Pool or Not to Pool? (row aggregation in cut generation)"
authors: "Rex, Gleixner et al."
year: 0000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [cuts]
priority: ○
status: standard
tags: [paper, cuts]
---

# To Pool or Not to Pool? (row aggregation in cut generation)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Asks whether aggregating rows before cut generation (and pooling the results) helps or hurts cut quality — an empirical cut-management question.

## Metadata
| Field | Value |
|---|---|
| Authors | Rex, Gleixner et al. |
| Year | **not stated in reference list** — slug uses 0000 placeholder (see manifest) |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Row aggregation (as in #102) can expose stronger inequalities, but aggregates are denser and can duplicate or cancel information already present. Which side wins in a modern solver is an empirical cut-management decision.

## Core Contribution
- **Methodology:** Compare cut generation from raw rows vs. from aggregated rows, controlling for cut count and LP overhead; evaluate effect on gap closure and node time.
- **Assumptions:** Existing branch-and-cut with a cut pool; aggregation heuristics pre-specified.
- **Benchmarks/datasets:** (not stated in list) — expected MIPLIB-class instances.
- **Metrics:** Gap closure, cut density, LP time per node, nodes.
- **Key results:** (not stated in list; reference exists only as a one-line entry) — intended takeaway per list: "aggregation effects on cut quality".

## Engineering-Relevant Knowledge
**Algorithms:** Aggregate-then-separate vs. separate-per-row; pool deduplication.
**Techniques:** Row aggregation before MIR/cover separation; dominance/parallelism checks on pooled cuts.
**Implementation details:** We have a pool (`src/milp/cut_pool.cpp`) but no aggregation-before-separation step; presolve has only 4 rules, so aggregation is effectively absent. High-value, low-cost experiment.
**Equations/rules:** Aggregate μ ≥ 0 over rows, separate on aggregate (as in [[Marchand-1996-Aggregation-Knapsack-Inequalities]]), then test density penalty against LP solve cost.
**Limitations/failure cases:** Denser rows ⇒ fill-in in basis updates; source not fully identified in list — **verify bibliographic details before citing**.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Directly addresses a policy knob (aggregate or not) we do not yet have, and its question shape matches our open problem: cut machinery exists, but policy is untested.

## Evidence → Engineering Decision
- *Finding:* aggregation may improve or degrade cut quality → *PS requirement:* R5 → *Component:* src/presolve/presolve.cpp, src/milp/cut_pool.cpp → *Metric:* root gap, LP time per node ([[Cut Efficiency]])

## Related Papers
- [[Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Benichou-1997-Linear-Programming-Implementations]]
- [[Turner-2024-Potential-Cutting-Planes]]

## Uses
- [[Cut Pooling]] [[Mixed Integer Rounding Cut]] [[Cut Validity]] [[Weak Relaxation]]
