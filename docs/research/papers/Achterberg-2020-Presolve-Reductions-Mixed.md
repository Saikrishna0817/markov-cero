---
type: paper
title: "Presolve Reductions in Mixed Integer Programming"
authors: "Achterberg, Bixby, Gu, Rothberg & Weninger"
year: 2020
venue: "IJOC"
doi: "10.1287/ijoc.2018.0857"
domain: [presolve]
priority: ★
status: deep
tags: [paper, presolve]
---

# Presolve Reductions in Mixed Integer Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Complete Gurobi presolve taxonomy — the most complete modern description of what a production MIP presolve does.

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg, Bixby, Gu, Rothberg & Weninger |
| Year | 2020 |
| Venue | IJOC |
| DOI/URL | 10.1287/ijoc.2018.0857 |

## Problem Addressed
Commercial presolve is opaque; practitioners cannot know which reductions run, in what order, or what they are worth. The paper opens Gurobi's presolve: dozens of rules, their interactions, iteration control and measured contribution to solve time.

## Core Contribution
- **Methodology:** Full rule catalogue (singleton/doubleton, implied bounds, dual fixing, probing, aggregation, mixing/strengthening, dominating rows/columns, ...), organized as phases with pruning of redundant work; detailed cost/benefit accounting.
- **Assumptions:** MIP of general form; tolerance-based validity; reversible transformation stack for postsolve.
- **Benchmarks/datasets:** Large MIPLIB-derived benchmark sets (Gurobi internal + public).
- **Metrics:** Nonzeros/rows/cols removed, root LP time, total solve time, node count.
- **Key results:** Presolve delivers the single largest consistent speedup of any solver component; orders-of-magnitude on some instances (qualitative — exact percentages are in-paper).

## Engineering-Relevant Knowledge
**Algorithms:** MIP presolve pipeline; dual fixing via LP duals; probing with implication graph.
**Techniques:** Rule scheduling/short-circuiting (skip rules when no reductions found); scaling interaction (presolve before [[Ruiz Scaling]] order matters).
**Implementation details:** Our presolve implements ~4 elementary rules (`PresolveStatistics` in `include/markov_cero/presolve/presolve.hpp`); this paper is the backlog. Probing, dual fixing, aggregation and mixing are all missing. Gurobi's phases give a safe ordering blueprint for adding them.
**Limitations/failure cases:** Aggressive reductions can degrade conditioning (larger coefficients); over-aggressive fixing of near-degenerate variables risks wrong answers beyond tolerance (R17 correctness risk).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 explicitly lists presolve; R20 wants industrial-scale performance. Presolve is the highest-leverage gap in markov-cero relative to commercial solvers, and this paper is the prioritized spec.

## Evidence → Engineering Decision
- *Finding:* Production value concentrates in probing, dual fixing and aggregation — none implemented → *PS requirement:* R5 → *Component:* src/presolve/presolve.cpp (+ presolve_stack.hpp for new transformation records) → *Metric:* root LP time; nodes explored (evidence/miplib_results.csv).
- *Finding:* Presolve must run before scaling and root cuts, with postsolve undoing everything → *PS requirement:* R5, R9 → *Component:* src/milp/milp_solver.cpp call order + postsolve → *Metric:* dual feasibility after postsolve; verifier pass rate.

## Related Papers
- [[Andersen-1995-Presolving-Linear-Programming]]
- [[Gamrath-2015-Progress-Presolving-Mixed]]
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Wang-2026-Enhancing-Presolve-Mixed]]

## Uses
- [[Presolve]]
- [[Reduced Cost]]
- [[Degeneracy]]
- [[Ruiz Scaling]]
