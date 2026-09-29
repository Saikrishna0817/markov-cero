---
type: paper
title: "On the Potential of Cutting Planes in Practical MIP / Learning to Select Cutting Planes"
authors: "Turner, Berger & Achterberg, 2024; Papageorgiou & Trespalacios"
year: 2024
venue: "(not stated in list)"
doi: "(unverified)"
domain: [cuts]
priority: ○
status: standard
tags: [paper, cuts]
---

# On the Potential of Cutting Planes in Practical MIP / Learning to Select Cutting Planes

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Modern view that cut *management and selection*, not cut generation, is where the remaining MIP speedups live.

## Metadata
| Field | Value |
|---|---|
| Authors | Turner, Berger & Achterberg (2024); Papageorgiou & Trespalacios (second, merged entry) |
| Year | 2024 (first paper); second paper year not given in list |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Solvers already generate many cut families, yet adding more cuts often slows the solve. Which cuts to keep, where to apply them, and how many to add per node is under-studied relative to inventing new families.

## Core Contribution
- **Methodology:** Quantify how much a solver could gain from better cut decisions (a "potential" study), and frame cut choice as a selection/ranking problem amenable to learned or heuristic scoring.
- **Assumptions:** Existing branch-and-cut engine with instrumentation; standard MIP benchmarks.
- **Benchmarks/datasets:** MIPLIB-style instances (not itemized in list).
- **Metrics:** Time/nodes with cuts vs. oracle-like cut decisions; per-family cut utility.
- **Key results:** Cut management is the highest-leverage remaining lever — consistent with our finding that generating GMI/MIR but applying them root-only yields 0.0% node reduction.

## Engineering-Relevant Knowledge
**Algorithms:** Cut selection/ranking, per-family utility estimation, adaptive cut limits per node.
**Techniques:** Score cuts by estimated dual bound improvement vs. LP cost; discard dominated/parallel cuts; tune limits dynamically.
**Implementation details:** `src/milp/cut_pool.cpp` today has no learned scoring — a simple deterministic score (violation, density, estimated dual improvement) is a low-cost first step toward the paper's premise.
**Equations/rules:** Selection score ≈ predicted dual-bound gain − λ·(cut density / LP cost); λ tuned on benchmark geometric means.
**Limitations/failure cases:** Learned selectors need training data and can mis-generalize across instance classes; over-tight cut limits hurt hard instances while helping easy ones.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Reframes our cuts problem as a policy problem (when/where/how many) rather than a missing-family problem, which is cheap to instrument and directly targets the 0.0% result.

## Evidence → Engineering Decision
- *Finding:* cuts generated but root-only → 0.0% node reduction → *PS requirement:* R5, R20 → *Component:* src/milp/cut_pool.cpp, src/milp/milp_solver.cpp → *Metric:* node reduction, [[Cut Efficiency]]
- *Finding:* selection policy drives the gain → *PS requirement:* R16 → *Component:* src/milp/cut_pool.cpp → *Metric:* [[Geometric Mean Runtime]] vs. comparison solver

## Related Papers
- [[Giallombardo-2025-Machine-Learning-Techniques]]
- [[Benichou-1997-Linear-Programming-Implementations]]
- [[Balas-1996-Gomory-Cuts-Revisited]]
- [[Padberg-2005-Classical-Cuts-Mixed]]

## Uses
- [[Cut Pooling]] [[Cut Efficiency]] [[Branch and Cut]] [[Weak Relaxation]]
