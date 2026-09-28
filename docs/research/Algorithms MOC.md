---
type: moc
tags: [moc, algorithms, research, traceability]
status: complete
date: 2026-09-25
---

# Algorithms MOC — all 17 algorithm notes

> One row per note in `docs/research/algorithms/`: which architecture layer it belongs to, which
> PS requirement demands it, which component note implements it today (or *missing*), and the
> papers that back it. Layer definitions: [[Architecture MOC]]. Requirement matrix: [[21-traceability]].

## Column key

- **Layer** — L0 model · L1 reduction · L2 continuous engines · L3 MILP · L4 numerics · L5 evaluation · L6 parallel/GPU (see [[Architecture MOC]]).
- **PS req.** — requirement from [[sih26119_problem_statement|SIH26119 Problem Statement]], graded in [[09-research-code-alignment]] §6.1.
- **Current code** — component stem in `docs/codebase/components/`, or *missing* when no implementation exists (verified against the vault, 2026-09-25).
- **Key papers** — 2–3 verified slugs from `docs/research/papers/` (202 notes).

## The table

| # | Algorithm | Layer | PS req. | Current code | Key papers |
|---|---|---|---|---|---|
| 1 | [[ADMM]] | L2 | R2, R3 | [[QP-ADMM-Engine]] | [[Vanderbei-1995-Symmetric-Indefinite-Systems]] · [[Unknown-2025-Overview-GPU-Based-First]] · [[Goldfarb-1983-Numerically-Stable-Dual]] |
| 2 | [[Branch and Bound]] | L3 | R5, R20 | [[BranchAndCut]] | [[Land-1960-Automatic-Method-Solving]] · [[Clausen-1999-Branch-Bound-Algorithms]] · [[Linderoth-2000-Impact-Branch-Bound]] |
| 3 | [[Branch and Cut]] | L3 | R5, R20 | [[BranchAndCut]] | [[Cornuejols-2001-Branch-and-Cut-Algorithms]] · [[Padberg-1991-Branch-and-Cut-Algorithm]] · [[Junger-1995-Branch-and-Cut-Combinatorial]] |
| 4 | [[Dual Simplex]] | L2 | R5, R20 | [[DualSimplexEngine]] | [[Koberstein-2005-Dual-Simplex-Method]] · [[Maros-2003-Generalized-Dual-Phase]] · [[Huangfu-2018-Parallelizing-Dual-Revised]] |
| 5 | [[Feasibility Pump]] | L3 | R5, R20 | [[PrimalHeuristics]] | [[Fischetti-2006-Feasibility-Pump-Heuristic]] · [[Achterberg-2007-Improving-Feasibility-Pump]] · [[Berthold-2023-Feasibility-Jump]] |
| 6 | [[Gomory Mixed Integer Cut]] | L3 | R5, R13 | [[CutGenerators]] | [[Gomory-1958-Outline-Algorithm-Integer]] · [[Balas-1996-Gomory-Cuts-Revisited]] · [[Cornuejols-2008-Valid-Inequalities-Mixed]] |
| 7 | [[Interior-Point Method]] | L2 | **R4 (GAP)** | *missing* | [[Karmarkar-1984-New-Polynomial-Time-Algorithm]] · [[Mehrotra-1992-Implementation-Primal-Dual-Interior]] · [[Wright-1997-Primal-Dual-IPM]] |
| 8 | [[Iterative Refinement]] | L4 | R9, R17 | *missing* | [[Gleixner-2015-Iterative-Refinement-Linear]] · [[Wilkinson-1963-Rounding-Errors-Algebraic]] · [[Cline-1979-Estimate-Condition-Number]] |
| 9 | [[Markowitz Pivoting]] | L4 | R6, R9 | *missing* (column-max only in [[SparseBasis-LU]]) | [[Markowitz-1957-Elimination-Form-Inverse]] · [[Suhl-1990-Fast-LU-Factorization]] · [[Duff-1986-Direct-Methods-Sparse]] |
| 10 | [[Mixed Integer Rounding Cut]] | L3 | R5, R11 | [[CutGenerators]] | [[Marchand-1996-Mixed-Integer-Rounding]] · [[Cornuejols-2008-Valid-Inequalities-Mixed]] · [[Atamturk-2003-Cover-Inequalities-Mixed]] |
| 11 | [[Primal-Dual Hybrid Gradient]] | L2 | R4, R8 | [[PDLP-Engine]] · [[GPU-PDHG-Engine]] | [[Lu-2025-cuPDLP-GPU-Implementation]] · [[Unknown-2025-Overview-GPU-Based-First]] |
| 12 | [[Pseudo-Cost Branching]] | L3 | R5, R20 | [[BranchAndCut]] | [[Achterberg-2005-Branching-Rules-Revisited]] · [[Achterberg-2007-Best-Estimate-Bound]] · [[Berthold-2006-Hybrid-Branching]] |
| 13 | [[Revised Simplex]] | L2 | R4 | [[RevisedSimplexEngine]] | [[Dantzig-1963-Linear-Programming-Extensions]] · [[Azulay-0000-Revised-Simplex-Method]] · [[Hall-2005-Hyper-sparsity-Revised-Simplex]] |
| 14 | [[Sparse LDL Factorization]] | L4 | R2, R6 | [[LDL-Factorization]] | [[Vanderbei-1995-Symmetric-Indefinite-Systems]] · [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]] · [[Wright-1997-Primal-Dual-IPM]] |
| 15 | [[Sparse LU]] | L4 | R6, R9 | [[SparseBasis-LU]] · [[DenseLU]] | [[Curtis-1972-Simplex-LU-Decomposition]] · [[Forrest-1972-Updating-Triangular-Factors]] · [[Duff-1986-Direct-Methods-Sparse]] |
| 16 | [[Steepest Edge]] | L2 | R13 | *missing* (admitted in `dual_simplex.hpp:12`) | [[Goldfarb-1977-Practicable-Steepest-Edge]] · [[Goldfarb-1992-Steepest-Edge-Simplex]] · [[Fourer-1994-Steepest-Edge-Simplexing]] |
| 17 | [[Strong Branching]] | L3 | R5 | [[BranchAndCut]] | [[Achterberg-2005-Branching-Rules-Revisited]] · [[Held-2006-Lookahead-Branching-Mixed]] · [[Berthold-2006-Hybrid-Branching]] |

## Rollup by layer

| Layer | Notes | Implemented | Missing |
|---|---|---|---|
| L2 continuous engines | 6 | Revised Simplex, Dual Simplex, PDHG, ADMM | **Interior-Point Method**, **Steepest Edge** |
| L3 MILP | 7 | B&B, B&C, GMI, MIR, Feasibility Pump, Strong Branching, Pseudo-Cost Branching | — (the gap is cut *placement* and depth, not cut types) |
| L4 numerics | 4 | Sparse LU, Sparse LDL | **Iterative Refinement**, **Markowitz Pivoting** |
| **Total** | **17** | **13** | **4** |

## Algorithms NOT in the code

Short list — research exists, no implementation, no component note:

1. [[Interior-Point Method]] + [[Crossover]] — R4 hard GAP; 0 code hits ([[No Interior-Point Engine]]). Tracked as [[ED-003-interior-point-required-by-ps]] / [[13-restart-point]] step 6.
2. [[Steepest Edge]] dual pricing — [[DualSimplexEngine]] runs `tableau_norm`; node LPs pay for it every MIP node ([[Bland-Only Pricing]], RW-7).
3. [[Iterative Refinement]] — no refinement, no condition estimation anywhere (AP-8); blocks the cheap half of R17 (RW-8).
4. [[Markowitz Pivoting]] / fill-reducing ordering — [[SparseBasis-LU]] uses column-max pivoting only, `perm_`/`pinv_` are identity ([[Fill-Reducing Ordering]], P2-4).

Related gaps that are *not* algorithms but block the same rows: [[Root-Only Cuts]] (separation placement), [[Degeneracy Handling Gap]], [[Mittelmann Coverage Gap]], [[Missing External Baseline Comparison]].

## How to use this table

- **Research → code:** pick a row, open the component stem (file:line evidence), then compare against the papers in the last column; the delta is one row of [[21-traceability]] §21.1.
- **Code → research:** start at a component note's *Research Justification* section and follow it back to the algorithm row — worked examples in [[Research-Code Traceability MOC]].
- **Priority:** which rows must move before 2026-09-30 is decided in [[15-roadmap]] (P0) and [[12-keep-remove-rebuild]] (RW-1…RW-10).

## Related

[[Architecture MOC]] · [[Research MOC]] · [[Codebase MOC]] · [[Datasets MOC]] · [[Evaluation MOC]] · [[21-traceability]] · [[09-research-code-alignment]]
