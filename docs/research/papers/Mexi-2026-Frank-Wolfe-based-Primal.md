---
type: paper
title: "A Frank-Wolfe-based Primal Heuristic for Quadratic Mixed-Integer Optimization"
authors: "Mexi, Hendrych, Designolle, Besançon & Pokutta"
year: 2026
venue: "MPC"
doi: "10.1007/s12532-026-00267-2"
domain: [heuristics]
priority: ✦
status: standard
tags: [paper, heuristics]
---

# A Frank-Wolfe-based Primal Heuristic for Quadratic Mixed-Integer Optimization

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> First real MIQP primal heuristic: conditional-gradient descent on the quadratic relaxation, then rounding — 1st place Land-Doig MIP Competition 2025.

## Metadata
| Field | Value |
|---|---|
| Authors | Mexi, Hendrych, Designolle, Besançon & Pokutta |
| Year | 2026 |
| Venue | Mathematical Programming Computation (list: "MPC") |
| DOI/URL | 10.1007/s12532-026-00267-2 |

## Problem Addressed
Primal heuristics for MILP (FP, RINS) do not transfer to MIQP: projections onto a quadratic objective are not LPs, and general-integer quadratic models (our MIQP scope) have no incumbent machinery at all.

## Core Contribution
- **Methodology:** Apply the Frank–Wolfe (conditional gradient) algorithm to the continuous relaxation of the MIQP using linear minimization oracles over the feasible set, then round/repair to get integer candidates; extension of the Boscia framework to heuristic use.
- **Assumptions:** Convex (or tractably handled) quadratic objective; linear-minimization oracle; bounded feasible set.
- **Benchmarks/datasets:** MIQP instances; won 1st place in the Land-Doig MIP Competition 2025 (per list).
- **Metrics:** Incumbent quality/time on MIQP; competition ranking.
- **Key results:** Demonstrates that a first-order method can serve as a *heuristic* engine for MIQP — relevant because our QP/MIQP path is ADMM-based (`src/qp/admm_solver.cpp`) and has no integer heuristic.

## Engineering-Relevant Knowledge
**Algorithms:** Frank–Wolfe descent + rounding; conditional-gradient iterations with LMO.
**Techniques:** Using a continuous optimizer to generate incumbent candidates for the integer layer; warm-starting.
**Implementation details:** Our MIQP is ADMM over quasi-definite KKT (`src/qp/admm_solver.cpp`); a Frank–Wolfe variant would need an LMO over the QP feasible set — a larger change. First note the gap: **no MIQP primal heuristic exists in `src/milp/heuristics.cpp`**.
**Equations/rules:** x_{k+1} = argmin over linearized objective ⟨∇f(x_k), s⟩ + (1/τ_k)‖s − x_k‖²-style step (FW step size 2/(k+2) standard); then round.
**Limitations/failure cases:** FW converges slowly near optimum (sublinear); heuristic quality depends on the rounding step; nonconvex MIQP not covered.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Addresses the MIQP extension path (R3) where we currently have zero primal heuristics; not needed for the initial LP/MILP/QP scope.

## Evidence → Engineering Decision
- *Finding:* no heuristic exists on the QP/MIQP path → *PS requirement:* R2, R3 → *Component:* src/qp/admm_solver.cpp, src/milp/heuristics.cpp → *Metric:* MIQP incumbent quality, [[Relative Optimality Gap]]

## Related Papers
- [[Fischetti-2005-Feasibility-Pump]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Berthold-2025-Primal-Heuristics-Mixed]]
- [[Berthold-2023-Feasibility-Jump]]

## Uses
- [[Feasibility Pump]] [[Rounding Heuristic]] [[KKT Conditions]] [[LP Relaxation]]
