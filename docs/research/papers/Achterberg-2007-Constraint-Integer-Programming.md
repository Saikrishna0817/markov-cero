---
type: paper
title: "Constraint Integer Programming"
authors: "Achterberg"
year: 2007
venue: "ZIB (PhD thesis)"
doi: "(unverified)"
domain: [survey, milp]
priority: ★
status: deep
tags: [paper, survey, milp]
---

# Constraint Integer Programming

> The architectural blueprint for a MIP solver: presolve, cuts, heuristics, branching and the LP solver as tightly coupled components.

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg |
| Year | 2007 |
| Venue | ZIB (PhD thesis) |
| DOI/URL | (unverified) |

## Problem Addressed
MIP solvers assemble dozens of components whose interactions are rarely documented in one place; an implementer must know how presolve, cut generation, primal heuristics, branching rules and the LP relaxation solver feed each other inside branch-and-bound/branch-and-cut. The thesis defines that architecture end-to-end through the construction of SCIP.

## Core Contribution
- **Methodology:** Design and implementation of a branch-and-cut solver from mathematical foundations, with component-level design rationale and experiments (approximate).
- **Assumptions:** Mixed 0-1/integer variables with linear constraints; LP solver treated as a replaceable subroutine.
- **Benchmarks/datasets:** MIPLIB instances (approximate; no numbers asserted).
- **Metrics:** Nodes, solve time, gaps closed by cuts/heuristics (qualitative).
- **Key results:** A modular branch-and-cut architecture in which presolve, cuts, heuristics and branching each deliver visible, independently measurable gains (approximate; no figures asserted).

## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-bound, branch-and-cut, cutting-plane separation (Gomory, MIR, cover), LP relaxation solves at each node.

**Techniques:** Presolve with dual recovery, probing, conflict analysis, diving and other primal heuristics, strong branching, advanced node selection.

**Implementation details:** Directly suggests our module split: src/presolve/presolve.cpp, src/milp/gomory.cpp, src/milp/mir.cpp, src/milp/heuristics.cpp, src/milp/strong_branching.cpp, src/milp/node_lp.cpp, src/milp/work_queue.cpp — and mandates a per-component ablation harness.

**Equations/rules:** Node LP relaxation supplies bounds; cuts must preserve the integer hull (Cut Validity); incumbents drive pruning via the relative gap.

**Limitations/failure cases:** Thesis-era design predates GPU work and modern LP interiors; SCIP-specific engineering choices are not automatically optimal for us (approximate).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the reference decomposition for R3/R5 (modular MILP architecture with presolve, cuts, heuristics, branch-and-cut) and shows how the LP engine, presolve and cut pool must exchange information; our repo already mirrors this layout.

## Evidence → Engineering Decision
- *Finding:* MIP performance comes from component interaction, not any single algorithm → *PS requirement:* R5 → *Component:* src/milp/milp_solver.cpp → *Metric:* Relative Optimality Gap
- *Finding:* Every component must be switchable for ablation and comparison runs → *PS requirement:* R16 → *Component:* benchmarks/ → *Metric:* Geometric Mean Runtime

## Related Papers
- [[Bixby-2002-Evolution-of-LP]]
- [[Kumar-2010-Fifty-Years-Integer]]
- [[Hoffman-1991-Improving-LP-Representations]]
- [[Nemhauser-1988-Integer-Combinatorial-Optimization]]

## Uses
- [[LP Relaxation]]
- [[Presolve]]
- [[Cut Validity]]
