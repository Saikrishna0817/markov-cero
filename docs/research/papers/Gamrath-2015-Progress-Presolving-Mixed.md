---
type: paper
title: "Progress in Presolving for Mixed Integer Programming"
authors: "Gamrath, Koch, Martin, Miltenheimer & Weninger"
year: 2015
venue: "MPC (Math. Prog. Comp.)"
doi: "(unverified)"
domain: [presolve]
priority: ★
status: deep
tags: [paper, presolve]
---

# Progress in Presolving for Mixed Integer Programming

> New-generation MIP presolve rules: singleton-column stuffing, dominating columns, connected components.

## Metadata
| Field | Value |
|---|---|
| Authors | Gamrath, Koch, Martin, Miltenheimer & Weninger |
| Year | 2015 |
| Venue | MPC (Mathematical Programming Computation) |
| DOI/URL | (unverified) |

## Problem Addressed
Two decades after Savelsbergh, presolve still missed reductions that materially shrink difficult MIPs: constraints that can be *stuffed* into a single column, columns that dominate others, and models that decompose into independent components. The paper adds and evaluates these.

## Core Contribution
- **Methodology:** Singleton-column stuffing (move row contribution into one column so a row disappears), dominating-column elimination (a column no worse than another can be fixed/removed), connected-component detection to split the model into independent sub-MIPs.
- **Assumptions:** MIP in standard form; bound-consistent dominance tests; component graph over constraints/variables.
- **Benchmarks/datasets:** MIPLIB-class MIPs.
- **Metrics:** Rows/cols/nonzeros removed; nodes; solve time.
- **Key results:** Each rule gives measurable additional reductions on top of classical presolve; components allow parallel independent solves (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Stuffing, dominance, model decomposition.
**Techniques:** Aggregation of rows into columns; graph-based component split.
**Implementation details:** Components are directly parallelizable — aligns with existing `src/milp/parallel_tree_search.cpp` (C++20 jthread) and R7. All three rules are absent from our 4-rule presolve.
**Limitations/failure cases:** Stuffing can increase coefficient range ([[Ill-Conditioning]]); dominance needs careful tolerance handling; components help only when the model is genuinely decomposable.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 (presolve), R7 (multi-core via component parallelism), R20 (runtime). Component splitting is the cheapest structural parallelism available before touching the tree search.

## Evidence → Engineering Decision
- *Finding:* Connected components admit independent solves → *PS requirement:* R5, R7 → *Component:* src/presolve/presolve.cpp (component detection) + src/milp/parallel_tree_search.cpp → *Metric:* wall time on decomposable MIPLIB instances; nodes explored.
- *Finding:* Stuffing/dominance shrink the LP relaxation beyond current rules → *PS requirement:* R5, R13 (weak relaxations) → *Component:* src/presolve/presolve.cpp → *Metric:* root gap ([[Relative Optimality Gap]]).

## Related Papers
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
- [[Andersen-1995-Presolving-Linear-Programming]]
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques]]

## Uses
- [[Presolve]]
- [[Sparsity]]
- [[LP Relaxation]]
