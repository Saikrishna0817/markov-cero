---
type: paper
title: "Computational Study of Cutting Planes for a Lot-Sizing Problem in Branch-and-Cut Algorithm"
authors: "Chung"
year: 2015
venue: "J. Operations Research Society of Korea"
doi: "10.7737/jkorms.2015.40.3.023"
domain: [cuts]
priority: ✦
status: standard
tags: [paper, cuts]
---

# Computational Study of Cutting Planes for a Lot-Sizing Problem in Branch-and-Cut Algorithm

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Application-level head-to-head comparison of three cut families on lot-sizing, with the practical lesson that family choice is instance-structure dependent.

## Metadata
| Field | Value |
|---|---|
| Authors | Chung |
| Year | 2015 |
| Venue | Journal of the Operations Research Society of Korea |
| DOI/URL | 10.7737/jkorms.2015.40.3.023 |

## Problem Addressed
Cut papers usually argue for one family in isolation. Lot-sizing (a planning structure inside our R11 scope: production planning) lets you measure how three families compare on the same rows and the same LP engine.

## Core Contribution
- **Methodology:** Implement three cut families inside a branch-and-cut for a lot-sizing formulation and compare strength/cost empirically (families compared in the paper; list records "strength comparison of three cut families (application-level)").
- **Assumptions:** Single/commodity lot-sizing formulation; identical LP and node policy across variants.
- **Benchmarks/datasets:** Lot-sizing instances (Korean ORS journal; not itemized in list).
- **Metrics:** Root gap closure, node counts, runtime per family.
- **Key results:** No family dominates universally — measurable, per-instance differences; supports instrumenting our own cut families separately rather than enabling all at once.

## Engineering-Relevant Knowledge
**Algorithms:** Per-family separation in branch-and-cut; controlled A/B comparison protocol.
**Techniques:** Isolate the cut variable: hold branching, heuristics and presolve fixed while swapping families (the correct way to measure cut value — applies directly to our R16 comparison).
**Implementation details:** Our `phase4.json` conflates cuts with placement (root-only). A controlled experiment: root-only vs. tree-wide application, all else equal, in `src/milp/milp_solver.cpp`.
**Equations/rules:** none (empirical study).
**Limitations/failure cases:** Domain-specific formulation; three families only; small instances may not reveal tree-level effects.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Its experimental design (one variable changed, families measured separately) is exactly what we need to explain the 0.0% node reduction; lot-sizing also sits in our planning/refinery scope (R11, R19).

## Evidence → Engineering Decision
- *Finding:* cut families must be measured in isolation to attribute gains → *PS requirement:* R16, R20 → *Component:* src/milp/milp_solver.cpp, evidence/benchmarks → *Metric:* [[Cut Efficiency]], [[Geometric Mean Runtime]]

## Related Papers
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Balas-1980-Cuts-Fixed-Rank]]
- [[Turner-2024-Potential-Cutting-Planes]]
- [[Atamturk-2003-Cover-Inequalities-Mixed]]

## Uses
- [[Cut Efficiency]] [[MIPLIB]] [[Weak Relaxation]] [[Branch and Cut]]
