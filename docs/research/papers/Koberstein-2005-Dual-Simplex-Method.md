---
type: paper
title: "The Dual Simplex Method: Techniques for a Fast and Stable Implementation"
authors: "Koberstein"
year: 2005
venue: "PhD thesis"
doi: "(unverified)"
domain: [lp]
priority: ★
status: deep
tags: [paper, lp]
---

# The Dual Simplex Method: Techniques for a Fast and Stable Implementation

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The definitive dual-simplex implementation reference: Harris tolerances, BFRT, hypersparse solves and pricing loops.

## Metadata
| Field | Value |
|---|---|
| Authors | Koberstein |
| Year | 2005 |
| Venue | PhD thesis |
| DOI/URL | http://nbn-resolving.de/urn:nbn:de:hbz:466-20050101272 |

## Problem Addressed
MIP solvers re-optimize the LP relaxation at every tree node, and the dual simplex is the algorithm of choice because it warm-starts from an existing basis. Yet the knowledge needed to make dual simplex fast *and* stable — ratio-test tolerances, bound-flipping, hypersparse solves, pricing loops — was scattered across papers and solver internals; the thesis consolidates it into one implementable design.

## Core Contribution
- **Methodology:** Consolidates and systematically analyzes the practical techniques of the dual revised simplex, integrating Harris tolerances, BFRT bound-flipping, hypersparse linear algebra and partial pricing into one coherent implementation (approximate).
- **Assumptions:** LP in standard form; existing basis to warm-start from; degenerate/infeasible starting bases are the norm in MIP node LPs (approximate).
- **Benchmarks/datasets:** Netlib-class LPs (approximate; no instance list asserted).
- **Metrics:** Iterations, time per iteration, refactorizations, failures on degenerate models (qualitative).
- **Key results:** The combination of these techniques yields a dual simplex robust on degenerate problems at near state-of-the-art speed (qualitative; no figures asserted).

## Engineering-Relevant Knowledge
**Algorithms:** Dual revised simplex with partial pricing and refactorization policy.

**Techniques:** Harris Ratio Test, BFRT bound-flipping row technique, hypersparse FTRAN/BTRAN, steepest-edge pricing with weight updates, expanded-tolerance anti-cycling (EXPAND-style).

**Implementation details:** This is the working specification for src/lp/dual/dual_simplex.cpp, which runs at every MIP node (src/milp/node_lp.cpp) — so per-iteration cost multiplies directly into tree time (R20); price/ratio loops must exploit hyper-sparsity rather than scan full rows.

**Equations/rules:** Dual ratio test minimizes objective increase over leaving candidates; BFRT lets several bound flips occur in one iteration when a bound blocks progress; Harris tolerances accept a slightly infeasible pivot to keep moving on degenerate vertices.

**Limitations/failure cases:** Thesis-era design is essentially sequential (see Huangfu & Hall for the parallel successor); Harris tolerances can accept infeasibility that must be caught by verification; weight drift requires periodic resets.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 needs a fast dual simplex at every MIP node, R13 needs degeneracy robustness and R9 needs reliable convergence — the thesis addresses all three with implementable detail, unlike algorithm-only descriptions.

## Evidence → Engineering Decision
- *Finding:* Node-LP performance is the product of dual-simplex iteration cost and tree size → *PS requirement:* R5 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* Geometric Mean Runtime
- *Finding:* Harris tolerances + BFRT are the practical answer to degenerate node LPs → *PS requirement:* R13 → *Component:* src/lp/dual/dual_simplex.cpp → *Metric:* KKT Residual

## Related Papers
- [[Goldfarb-1992-Steepest-Edge-Simplex]]
- [[Harris-1973-Pivot-Selection-Methods]]
- [[Gill-1989-Practical-Anti-Cycling]]
- [[Huangfu-2018-Parallelizing-Dual-Revised]]

## Uses
- [[Dual Simplex]] [[Harris Ratio Test]] [[Degeneracy]]
