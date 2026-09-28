---
type: paper
title: "General Mixed Integer Programming: Computational Issues for Branch-and-Cut Algorithms"
authors: "Achterberg, Koch & Martin"
year: 2005
venue: "IJOC"
doi: "(unverified)"
domain: [milp]
priority: ★
status: deep
tags: [paper, milp]
---

# General Mixed Integer Programming: Computational Issues for Branch-and-Cut Algorithms

> How every component (presolve, cuts, heuristics, branching, LP) interacts at a single node — the systems view of a MIP solver.

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg, Koch & Martin |
| Year | 2005 |
| Venue | IJOC |
| DOI/URL | (unverified) |

## Problem Addressed
Component papers evaluate features in isolation, but solver behavior emerges from interactions: cuts strengthen relaxations, change branching scores, alter heuristic success, and interact with degeneracy. The paper measures these couplings on general MIP.

## Core Contribution
- **Methodology:** Controlled computational study varying one component at a time (presolve on/off, cut families, heuristics, branching rules) on large MIP sets; reports node counts vs. LP time trade-offs.
- **Assumptions:** Full solver with switchable components; representative benchmark set (MIPLIB-class).
- **Benchmarks/datasets:** Large general MIP benchmark sets.
- **Metrics:** Nodes, LP iterations, time, solved count.
- **Key results:** Cuts and presolve dominate; heuristics reduce nodes though they cost time; branching rule choice has second-order but real effects (qualitative summary; exact numbers in paper).

## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut orchestration order: presolve -> root LP -> cuts -> heuristics -> branch.
**Techniques:** Component ablation methodology (the right way to justify features).
**Implementation details:** markov-cero has all named components but cuts are root-only and presolve is 4 rules; this paper ranks where to invest next: per-node separation and MIP presolve before new branching rules.
**Limitations/failure cases:** Findings are solver-era dependent; absolute percentages do not transfer to our stack (use as direction, not target).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R5 lists exactly this component set; R16 requires comparison methodology — ablation tables like this paper's are the template for our evidence reports.

## Evidence → Engineering Decision
- *Finding:* Presolve + cuts are the largest levers; our cuts are root-only → *PS requirement:* R5, R20 → *Component:* src/milp/cut_pool.cpp (node-level separation) → *Metric:* nodes explored; wall time (evidence/miplib_results.csv).
- *Finding:* Component ablation is the correct evaluation design → *PS requirement:* R16 → *Component:* tests/milp_test.cpp + CLI flags `--no-cuts`, `--no-presolve` → *Metric:* solved-count delta per flag.

## Related Papers
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Bixby-2020-Compiling-Mixed-Integer]]
- [[Andersen-1995-Presolving-Linear-Programming]]

## Uses
- [[Branch and Cut]]
- [[Gomory Mixed Integer Cut]]
- [[Presolve]]
- [[Relative Optimality Gap]]
