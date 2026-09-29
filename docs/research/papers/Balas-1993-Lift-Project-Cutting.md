---
type: paper
title: "A Lift-and-Project Cutting Plane Algorithm for Mixed 0/1 Programs"
authors: "Balas, Ceria & Cornuéjols"
year: 1993
venue: "Math. Prog."
doi: "10.1007/BF01581273"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# A Lift-and-Project Cutting Plane Algorithm for Mixed 0/1 Programs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Lift-and-project / disjunctive cuts: cut a 0/1 polytope by optimizing over each branch of a variable split and recombining the results.

## Metadata
| Field | Value |
|---|---|
| Authors | Balas, Ceria & Cornuéjols |
| Year | 1993 |
| Venue | Mathematical Programming (list: "Math. Prog.") |
| DOI/URL | 10.1007/BF01581273 |

## Problem Addressed
Cuts valid for one row are weak when the integrality structure is spread across a disjunction (x_j = 0 or x_j = 1). The paper asks how to exploit a binary split to generate inequalities that are valid for the union of the two resulting polyhedra.

## Core Contribution
- **Methodology:** For a chosen 0/1 variable, solve two LPs with the variable fixed to 0 and to 1, then convexly combine the two optimal extreme points/inequalities into a disjunctive cut valid for the whole set; iterate over variables.
- **Assumptions:** 0/1 (or bounded integer) variables; a solvable LP for each fixing; cuts collected in a pool and added in batches.
- **Benchmarks/datasets:** Small 0/1 test problems of the era; later used on set-covering/assignment models.
- **Metrics:** Root gap closed, LP solves required, number of cuts.
- **Key results:** Foundation of lift-and-project and of the split inequality; computationally expensive (two LPs per split variable) but produces provably stronger inequalities than single-row rounding.

## Engineering-Relevant Knowledge
**Algorithms:** Lift-and-project, disjunctive cuts, split cuts; variable selection for the disjunction.
**Techniques:** Fix-and-resolve ("what-if") evaluation — the same mechanism as trial [[Strong Branching]]; batch cut insertion with a cut pool.
**Implementation details:** Each split requires two LP solves — cost must be amortized; a `cut_pool.cpp` with a global cap and parallel separation is the practical route. Not implemented in our tree today.
**Equations/rules:** Disjunctive inequality: for polyhedra P_0, P_1, a cut valid on both, e.g. λ·(row valid on P_0) + (1−λ)·(row valid on P_1) with λ ∈ [0,1] fixed a priori (see #97 for the rank view).
**Limitations/failure cases:** O(#candidates × 2) LP solves; cuts can be dense and numerically delicate; benefit is mostly at/near the root, not deep in the tree.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Strong branching already performs the two LP solves this method needs, so a lift-and-project separator could reuse that work; but it is a second-generation cut system we do not yet need before fixing root-only cuts.

## Evidence → Engineering Decision
- *Finding:* disjunctive cuts need 2 LP solves per candidate → *PS requirement:* R5 → *Component:* src/milp/strong_branching.cpp, src/milp/cut_pool.cpp → *Metric:* runtime per cut, [[Cut Efficiency]]
- *Finding:* current cuts root-only, 0.0% node reduction → *PS requirement:* R5 → *Component:* src/milp/milp_solver.cpp → *Metric:* node-count reduction

## Related Papers
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy]]
- [[Balas-1980-Cuts-Fixed-Rank]]
- [[Benichou-1997-Linear-Programming-Implementations]]
- [[Padberg-2005-Classical-Cuts-Mixed]]

## Uses
- [[Strong Branching]] [[Cut Pooling]] [[LP Relaxation]] [[Cut Validity]]
