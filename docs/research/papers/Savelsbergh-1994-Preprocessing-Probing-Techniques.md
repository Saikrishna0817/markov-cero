---
type: paper
title: "Preprocessing and Probing Techniques for Mixed Integer Programming Problems"
authors: "Savelsbergh"
year: 1994
venue: "ORSA J. Computing"
doi: "(unverified)"
domain: [presolve, milp]
priority: ★
status: deep
tags: [paper, presolve]
---

# Preprocessing and Probing Techniques for Mixed Integer Programming Problems

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> MIP-specific preprocessing: probing on binaries, logical implications, clique inequalities and coefficient reduction.

## Metadata
| Field | Value |
|---|---|
| Authors | Savelsbergh |
| Year | 1994 |
| Venue | ORSA J. Computing |
| DOI/URL | (unverified) |

## Problem Addressed
LP presolve treats integers as bounds; MIP preprocessing can prove much more by exploiting integrality: if a binary is forced to a value under both settings of another, implications follow. The paper systematizes these techniques and their safe application.

## Core Contribution
- **Methodology:** Probing: fix a candidate binary to 0 and to 1, run LP bound propagation on both branches, deduce variable bound implications, fix variables, tighten coefficients; derive clique inequalities from mutually exclusive binaries; coefficient reduction on packing/covering rows.
- **Assumptions:** Binary/0-1 structure for probing; LP bound propagation oracle; tolerances for reported bounds.
- **Benchmarks/datasets:** MIP test problems of the era (paper reports iterations/time reductions).
- **Metrics:** Fixed variables, tightened bounds, LP iterations and time saved.
- **Key results:** Meaningful node/LP-iteration reductions on hard MIPs (qualitative).

## Engineering-Relevant Knowledge
**Algorithms:** Probing search with LP bound propagation; clique detection.
**Techniques:** Logical implications (`x=0 => y<=3`), coefficient reduction, clique table as cut generator.
**Implementation details:** Our presolve has zero MIP-specific rules (4 LP rules only); probing must run before the root LP and can also *feed* the cut manager — clique info naturally produces [[Cut Validity]] candidates. Cost control: probe only a small candidate subset (density/fractionality heuristics).
**Limitations/failure cases:** Probing is expensive (2 LP bound passes per candidate); on general integers it must be limited; tolerance errors create wrong fixes that appear only deep in the tree ([[Degeneracy]]).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 (presolve + cuts) and R20 (near-optimal solutions fast). Probing plus dual fixing is the standard route to stronger root relaxations and smaller trees — both measurable in evidence/miplib_results.csv (`nodes_explored`, `lp_iterations`).

## Evidence → Engineering Decision
- *Finding:* No probing/integrality-aware rule exists in our presolve → *PS requirement:* R5 → *Component:* src/presolve/presolve.cpp (MIP pass before src/milp/milp_solver.cpp tree) → *Metric:* nodes explored, root LP iterations (evidence/miplib_results.csv).
- *Finding:* Probing yields clique structures reusable by cuts → *PS requirement:* R5 → *Component:* src/milp/cut_pool.cpp → *Metric:* [[Cut Efficiency]] (gap closed per cut).

## Related Papers
- [[Andersen-1995-Presolving-Linear-Programming]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
- [[Wang-2026-Enhancing-Presolve-Mixed]]
- [[Atamturk-2008-Integer-Programming-Software]]

## Uses
- [[Presolve]]
- [[LP Relaxation]]
- [[Cut Validity]]
