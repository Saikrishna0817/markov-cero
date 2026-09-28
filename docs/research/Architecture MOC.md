---
type: moc
tags: [moc, architecture, solver, research]
status: complete
date: 2026-09-25
---

# Architecture MOC — layered solver architecture

> The solver as the literature describes it, layer by layer (model → reduction → engines →
> numerics → evaluation → parallel), each layer pinned to the codebase component that implements
> it today and to the papers that say what it should do. Siblings: [[Research MOC]] ·
> [[Codebase MOC]] · [[Algorithms MOC]] · [[Research-Code Traceability MOC]].

## How to read this map

- **Current code** — what `src/`, `gpu/` and `apps/` demonstrably do today; every component note
  carries `file:line` evidence (see [[07-current-architecture]] for the verified pipeline).
- **Research** — 2–3 paper notes per layer: the baseline the literature sets. The delta between
  the two lines is graded in [[09-research-code-alignment]] and traced row-by-row in [[21-traceability]].
- Layer numbering L0–L6 matches the diagram in [[14-target-architecture]], so "as built" and
  "as intended" can be read against the same skeleton.

## L0 — Model layer

Immutable canonical model: input formats, CSC storage, canonicalization consumed by every engine.
Research anchor: [[CSC Sparse Model]].

- **Current code:** [[MPSParser]] (`src/io/mps.cpp`) · [[Canonicalizer]] (`src/transform/*.cpp`) · [[csc-sparse-storage]] · [[Solve-Pipeline]].
- **Research:** [[Bell-2008-Efficient-Sparse-Matrix]] · [[Hall-2005-Hyper-sparsity-Revised-Simplex]] · [[Anderson-1989-Solving-Sparse-Linear]].
- **Gap:** two canonicalization paths behind an arbitrary 2048×8192 dense gate (AP-7, RW-5).

## L1 — Reduction layer

Presolve, scaling and the reversible postsolve stack. Research anchors: [[research/concepts/Presolve|Presolve]] · [[Scaling]].

- **Current code:** [[codebase/components/Presolve|Presolve]] (3 rule classes, LIFO postsolve) · [[RuizScaling]] · [[Presolve-Postsolve Stack]].
- **Research:** [[Achterberg-2020-Presolve-Reductions-Mixed]] · [[Savelsbergh-1994-Preprocessing-Probing-Techniques]] · [[OLeary-1981-Equilibrating-Both-Matrices]].
- **Gap:** rule *depth* (AP-9, RW-6) — the stack itself is correct architecture and is kept ([[12-keep-remove-rebuild]]).

## L2 — Continuous engines

Five families behind `--engine auto` dispatch ([[CLI-MarkovCeroSolve]]).

### Revised simplex — [[Revised Simplex]]
- **Current code:** [[RevisedSimplexEngine]] (`src/lp/reference/revised_simplex.cpp`) on [[SparseBasis-LU]], Bland anti-cycling ([[Bland Anti-Cycling]]).
- **Research:** [[Azulay-0000-Revised-Simplex-Method]] · [[Dantzig-1963-Linear-Programming-Extensions]] · [[Reid-1982-Sparsity-Exploiting-Variant]].

### Dual simplex — [[Dual Simplex]]
- **Current code:** [[DualSimplexEngine]] (`src/lp/dual/dual_simplex.cpp`) — warm start, [[Harris Ratio Test]]; pricing is `tableau_norm`, explicitly *not* steepest-edge.
- **Research:** [[Koberstein-2005-Dual-Simplex-Method]] · [[Goldfarb-1983-Numerically-Stable-Dual]] · [[Huangfu-2018-Parallelizing-Dual-Revised]].
- **Gap:** missing [[Steepest Edge]] pricing — the single biggest node-LP lever (RW-7).

### First-order PDHG — [[Primal-Dual Hybrid Gradient]]
- **Current code:** [[PDLP-Engine]] (CPU, `src/lp/first_order/pdlp.cpp`) · [[GPU-PDHG-Engine]] (`gpu/src/pdhg_step.cpp`); [[RuizScaling]] + [[Diagonal Preconditioning]].
- **Research:** [[Lu-2025-cuPDLP-GPU-Implementation]] · [[Unknown-2025-Overview-GPU-Based-First]].
- **Note:** first-order ≠ interior-point — see [[First-Order Accuracy Ceiling]] before claiming R4 with it.

### Interior-point — [[Interior-Point Method]] — **MISSING**
- **Current code:** none (`rg "interior.point|ipm|predictor-corrector|crossover"` over `src/`, `include/`, `apps/` → 0 hits; AP-1).
- **Research:** [[Karmarkar-1984-New-Polynomial-Time-Algorithm]] · [[Mehrotra-1992-Implementation-Primal-Dual-Interior]] · [[Wright-1997-Primal-Dual-IPM]], plus crossover [[Ye-1998-Crossover-Interior-Point]].
- **Gap:** R4 is a hard GAP — [[No Interior-Point Engine]] · [[No Crossover]]; decision note [[ED-003-interior-point-required-by-ps]].

### ADMM (QP/MIQP) — [[ADMM]]
- **Current code:** [[QP-ADMM-Engine]] (`src/qp/admm_solver.cpp`) over a quasi-definite KKT system factored by [[LDL-Factorization]]; MIQP rides [[BranchAndCut]] node LPs.
- **Research:** [[Vanderbei-1995-Symmetric-Indefinite-Systems]] · [[Unknown-2025-Overview-GPU-Based-First]] · [[Sparse LDL Factorization]].

## L3 — MILP layer

- **Current code:** [[BranchAndCut]] (`src/milp/milp_solver.cpp`) · [[CutGenerators]] (GMI + MIR, **root only**) · [[PrimalHeuristics]] (rounding + feasibility pump) · [[ParallelTreeSearch]] (best-bound heap, no stealing).
- **Research:** [[Cornuejols-2001-Branch-and-Cut-Algorithms]] · [[Achterberg-2005-Branching-Rules-Revisited]] · [[Padberg-1991-Branch-and-Cut-Algorithm]].
- **Cut & algorithm notes:** [[Branch and Bound]] · [[Branch and Cut]] · [[Gomory Mixed Integer Cut]] · [[Mixed Integer Rounding Cut]] · [[Cut Pooling]] · [[Cut Validity]] · [[Strong Branching]] · [[Pseudo-Cost Branching]] · [[Feasibility Pump]].
- **Gap:** root-only separation → 0.0% node reduction (AP-3, RW-1; [[Root-Only Cuts]]).

## L4 — Numerics layer

- **Current code:** [[SparseBasis-LU]] · [[DenseLU]] · [[numerical-policy-centralized]] (ADR-M0-03 tolerances) · pivot/condition guards inside [[DualSimplexEngine]] · [[IndependentVerifiers]].
- **Research:** [[Gleixner-2015-Iterative-Refinement-Linear]] · [[Bartels-1969-Simplex-LU-Decomposition]] · [[Cline-1979-Estimate-Condition-Number]] · [[Maros-2003-Computational-Optimization-Techniques]].
- **Gap:** no iterative refinement, no condition estimation (AP-8) — see [[Iterative Refinement]] · [[Ill-Conditioning]] · [[Numerical Stability]].

## L5 — Evaluation layer

- **Current code:** [[benchmark-suites]] (Netlib/MIPLIB/GPU ctest runners) · [[04-evidence-inventory]] · certificates emitted by [[Solution-JSON-Writer]].
- **Research (metrics):** [[Geometric Mean Runtime]] · [[Relative Optimality Gap]] · [[KKT Residual]] · [[Cut Efficiency]] · [[Parallel Speedup]] · [[Parallel Efficiency]] · [[Primal Residual]] · [[Numerical Error]].
- **Research (method):** [[Dolan-2002-Benchmarking-Optimization-Software]] · [[Mittelmann-n.d.-Benchmarks-Optimization-Software]].
- **Gap:** no external baseline at all ([[No External Baseline]]) — the P0 harness is [[ED-001-comparison-harness-before-new-algorithms]]; full plan in [[Evaluation MOC]].

## L6 — Parallel / GPU layer

- **Current code:** [[ParallelTreeSearch]] + `ThreadSafeNodeQueue` (shared heap) · [[GPU-PDHG-Engine]] device-resident loop with [[GPU CSR SpMV]] kernels.
- **Research:** [[Huangfu-2018-Parallelizing-Dual-Revised]] · [[Berthold-2019-Parallel-SCIP-UG]] · [[Eckstein-1994-Control-Strategies-Parallel]] · [[Lu-2025-cuPDLP-GPU-Implementation]].
- **Technique notes:** [[Work Stealing]] · [[GPU CSR SpMV]] · [[Diagonal Preconditioning]] · [[Fill-Reducing Ordering]] · [[Symbolic Factorization]].
- **Gap:** no work stealing → 4 threads measure 0.56× (AP-4, RW-2; [[Negative Parallel Scaling]]).

## Cross-layer spine

One line, in execution order: `MPS → canonicalize → presolve + Ruiz → engine dispatch → postsolve → independent verifiers → JSON`.
Data-flow detail with line references: [[Solve-Pipeline]] · [[07-current-architecture]] §C.1.

## Related MOCs

- [[Algorithms MOC]] — all 17 algorithm notes, layer and R-number per row.
- [[Datasets MOC]] · [[Evaluation MOC]] — what runs on top of this architecture.
- [[Codebase MOC]] — the component notes behind every **Current code** line above.
- [[Research-Code Traceability MOC]] — how to walk any of these links in both directions.

## Where this is going

[[14-target-architecture]] — the same six layers with the gaps closed (new items marked *NEW*,
diff table Current → Target, and the explicit non-goals).
