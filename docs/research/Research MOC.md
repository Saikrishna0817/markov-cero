---
type: moc
tags: [moc, research]
status: stable
verified_on: 2026-09-25
---

# Research MOC — markov-cero knowledge graph

> Map of content for `docs/research/`: the concept layer (72 notes across 8 folders), the 204-paper corpus behind it, and the audit trail that grades this solver against SIH26119.

## Orientation

Read these ★ survey/backbone papers first — they contain most of what the concept layer summarizes:

- [[Bixby-2002-Evolution-of-LP]] — decomposes 40 years of LP speedups into presolve, sparse LU, dual simplex and steepest-edge; the priority list for any from-scratch engine.
- [[Wright-2004-Interior-Point-Revolution]] — history and theory linking simplex, barrier and Karmarkar; context for the [[No Interior-Point Engine]] gap.
- [[Achterberg-2007-Constraint-Integer-Programming]] — the architectural blueprint (presolve, cuts, heuristics, branching, LP interconnection) this solver is graded against.
- [[Maros-2003-Computational-Optimization-Techniques]] — the numerics reference for LP; pairs with [[Numerical Stability]] and [[Ill-Conditioning]].

## Layers

Concept-layer folders (one note per vocabulary term; 30–55 lines each, formula + repo evidence + R-number traceability):

- **concepts/** — [[Degeneracy]] · [[Ill-Conditioning]] · [[KKT Conditions]] · [[LP Relaxation]] · [[Basis]] · [[Crossover]]
- **algorithms/** — [[Revised Simplex]] · [[Dual Simplex]] · [[Branch and Cut]] · [[Interior-Point Method]] · [[Primal-Dual Hybrid Gradient]] · [[Gomory Mixed Integer Cut]]
- **techniques/** — [[Harris Ratio Test]] · [[Bland Anti-Cycling]] · [[Ruiz Scaling]] · [[Fill-Reducing Ordering]] · [[Work Stealing]] · [[GPU CSR SpMV]]
- **datasets/** — [[MIPLIB]] · [[Netlib LP Collection]] · [[Mittelmann Benchmarks]] · [[QPLIB]]
- **metrics/** — [[Geometric Mean Runtime]] · [[Relative Optimality Gap]] · [[KKT Residual]] · [[Parallel Speedup]] · [[Cut Efficiency]]
- **architectures/** — [[Multi-Engine Solver Architecture]] · [[Presolve-Postsolve Stack]] · [[CSC Sparse Model]]
- **limitations/** — [[Root-Only Cuts]] · [[No Crossover]] · [[Negative Parallel Scaling]] · [[No External Baseline]]
- **research-gaps/** — [[Missing External Baseline Comparison]] · [[No Interior-Point Engine]] · [[GPU Benefit Unproven]] · [[Ill-Conditioned Instance Dossier]]

## Modules 0–16

Module headers follow `docs/research_paper_references.md` (204 entries — deliberately not restated here; the table maps each module to its concept-layer entry points).

| # | Module | Concept-layer entry points |
|---|---|---|
| 0 | Orientation & Survey | [[Bixby-2002-Evolution-of-LP]], [[Achterberg-2007-Constraint-Integer-Programming]] |
| 1 | Problem Input, Data Structures, Scaling | [[Scaling]], [[CSC Sparse Model]], [[Ruiz Scaling]] |
| 2 | Sparse Linear Algebra Foundation | [[Sparsity]], [[Sparse LU]], [[Fill-Reducing Ordering]], [[Symbolic Factorization]] |
| 3 | LP: Simplex Methods | [[Revised Simplex]], [[Dual Simplex]], [[Steepest Edge]], [[Bland Anti-Cycling]] |
| 4 | LP: Interior-Point Methods | [[Interior-Point Method]], [[Crossover]], [[No Interior-Point Engine]] |
| 5 | Presolve & Model Reduction | [[Presolve]], [[Presolve-Postsolve Stack]], [[Weak Relaxation]] |
| 6 | Quadratic Programming (QP) | [[ADMM]], [[Sparse LDL Factorization]], [[QPLIB]] |
| 7 | MILP: Branch-and-Cut & Architecture | [[Branch and Bound]], [[Branch and Cut]], [[Multi-Engine Solver Architecture]] |
| 8 | Cutting Planes | [[Gomory Mixed Integer Cut]], [[Mixed Integer Rounding Cut]], [[Cut Validity]] |
| 9 | Primal Heuristics | [[Feasibility Pump]], [[Rounding Heuristic]], [[Diving]] |
| 10 | Branching & Node Selection | [[Strong Branching]], [[Pseudo-Cost Branching]], [[Warm Start]] |
| 11 | Numerical Robustness | [[Ill-Conditioning]], [[Numerical Stability]], [[Iterative Refinement]] |
| 12 | Parallelization & GPU | [[Work Stealing]], [[GPU CSR SpMV]], [[Parallel Speedup]] |
| 13 | Benchmarking, Evaluation, Reporting | [[MIPLIB]], [[Geometric Mean Runtime]], [[Mittelmann Benchmarks]] |
| 14 | Domain Applications (refinery, power, logistics) | [[LP Relaxation]], [[Branch and Cut]] — R11 industrial scope is stated in the problem statement note |
| 15 | Advanced Extensions (MIQP, NLP, MINLP) | [[ADMM]], [[Multi-Engine Solver Architecture]] (R3 extensibility) |
| 16 | Machine Learning for Solver Enhancement | (optional track — no concept notes; see module header in the reference index) |

## Four Sources of Truth

Every note in this vault must say which of these it is speaking from (labels from the audit):

- **[A] Problem statement** — what SIH/MRPL actually asked: [[sih26119_problem_statement|SIH26119 Problem Statement]] (R1–R20 checklist inside).
- **[B] Research literature** — what the 204 collected references suggest: `docs/research_paper_references.md` (module index above), notes in `docs/research/papers/`.
- **[C] Current codebase** — what the repository demonstrably does: component notes such as [[RevisedSimplexEngine]], [[BranchAndCut]], [[Presolve]], with file:line evidence.
- **[D] Final solution** — what should be built: defined by the audit phases, seeded from [[00-ground-truth]].

## Audit Reports

- [[00-ground-truth]] — Phase 0: the four sources, R1–R20 status table, the divergence table and the PS-GAP inventory (PS-GAP-01…06). Read before trusting any claim in `docs/`.

## Reading Paths

**Path 1 — New solver engineer (how the thing works):**
[[sih26119_problem_statement|SIH26119 Problem Statement]] → [[CSC Sparse Model]] → [[Basis]] → [[Revised Simplex]] → [[Dual Simplex]] → [[LP Relaxation]] → [[Branch and Cut]] → [[Presolve]]

**Path 2 — Numerical robustness reviewer (can I believe the numbers):**
[[Ill-Conditioning]] → [[Numerical Stability]] → [[Degeneracy]] → [[Harris Ratio Test]] → [[Iterative Refinement]] → [[KKT Residual]] → [[Ill-Conditioned Instance Dossier]] → [[Harris-1973-Pivot-Selection-Methods]]

**Path 3 — SIH evaluator (does it meet the brief):**
[[00-ground-truth]] → [[No External Baseline]] → [[Missing External Baseline Comparison]] → [[Mittelmann Coverage Gap]] → [[No Interior-Point Engine]] → [[GPU Benefit Unproven]] → [[Negative Parallel Scaling]] → [[Geometric Mean Runtime]]
