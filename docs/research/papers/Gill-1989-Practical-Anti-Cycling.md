---
type: paper
title: "Practical Anti-Cycling Procedure (EXPAND)"
authors: "Gill, Murray, Saunders & Wright"
year: 1989
venue: "Math. Prog."
doi: "(unverified)"
domain: [numerics]
priority: ★
status: deep
tags: [paper, numerics]
---

# Practical Anti-Cycling Procedure (EXPAND)

> Expanding working tolerances (EXPAND) prevent the simplex from stalling on degenerate vertices without Bland's prohibitively slow rule.

> **Collision note:** the canonical slug `Gill-1989-Practical-Anti-Cycling` is occupied by the Module 3 note for list entry **#44** (same paper, full title "A Practical Anti-Cycling Procedure for Linearly Constrained Optimization"). This file covers Module 11 entry **#143**. Merge candidate — see manifest.

## Metadata
| Field | Value |
|---|---|
| Authors | Gill, Murray, Saunders & Wright |
| Year | 1989 |
| Venue | Mathematical Programming (list: "Math. Prog.") |
| DOI/URL | (unverified; EXPAND PDF in list: convexoptimization.com/sol/papers/EXPAND.pdf) |

## Problem Addressed
Bland's rule guarantees termination but forces erratic, slow pivoting; strict tolerances make the simplex accept a degenerate pivot and repeat it forever. A production code needs anti-cycling that is neither pathological (Bland) nor unsafe (blind tolerance loosening).

## Core Contribution
- **Methodology:** Detect stalling from pivot/objective patterns, expand the working feasibility/optimality tolerance ε so the algorithm can move off the degenerate vertex, then contract ε back toward its nominal value when progress resumes — controlled infeasibility with an explicit cap.
- **Assumptions:** Stalling detectable; expansion bounded so final accuracy is preserved; nominal tolerances recovered after the stall clears.
- **Benchmarks/datasets:** Degenerate LPs (NETLIB-class).
- **Metrics:** Iterations to optimum on degenerate problems; accuracy of the final solution.
- **Key results:** Practical anti-stalling with accuracy control — the recommended production alternative to pure Bland anti-cycling; still guarantees finite termination when combined with an eventual Bland fallback.

## Engineering-Relevant Knowledge
**Algorithms:** EXPAND tolerance expansion; stall detection; [[Bland Anti-Cycling]] as fallback.
**Techniques:** Bounded working-tolerance growth; tolerance recovery; capping expansion so declared feasibility tolerances are never silently violated.
**Implementation details:** Our revised simplex is **Bland-only** (`src/lp/reference/revised_simplex.cpp`, audit C.3) and the dual simplex (`src/lp/dual/dual_simplex.cpp`) runs at every MIP node — both are EXPAND candidates. Any expansion must be re-checked by `src/verify/primal_verifier.cpp`.
**Equations/rules:** On stall: ε ← ε·(1+α); on progress: ε ← ε₀; hard cap ε ≤ ε_max so ‖Ax − b‖ stays within the declared tolerance. Recovery must precede accepting an "optimal" status.
**Limitations/failure cases:** Over-expansion accepts infeasible optima (verification must catch it); false stall triggers on legitimately slow models; interacts with ratio-test tie handling (#161/#162).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R13 explicitly requires handling "highly degenerate models" and R9 demands reliable convergence; EXPAND is a small, self-contained replacement for our slow Bland-only fallback — measurable as iterations-on-degenerate-LPs before/after.

## Evidence → Engineering Decision
- *Finding:* Bland-only anti-cycling; no tolerance expansion → *PS requirement:* R9, R13, R17 → *Component:* src/lp/reference/revised_simplex.cpp, src/lp/dual/dual_simplex.cpp → *Metric:* iterations on degenerate LPs, [[Numerical Error]]
- *Finding:* any ε expansion changes reported feasibility → *PS requirement:* R17 → *Component:* src/verify/primal_verifier.cpp → *Metric:* feasibility violation of returned bases ([[KKT Residual]])

## Related Papers
- [[Charnes-1954-Optimality-Multi-Valuedness]]
- [[Maros-1993-Practical-Anti-Degeneracy]]
- [[DeFarias-2019-Positive-Edge-Pricing]]
- [[Gill-1974-Methods-Modifying-Matrix]]

## Uses
- [[Bland Anti-Cycling]] [[Degeneracy]] [[Harris Ratio Test]] [[Dual Simplex]]
