---
type: concept
tags: [concepts, milp]
status: stable
verified_on: 2026-09-25
---

# Weak Relaxation

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> An LP bound that barely constrains the integer problem — the gap the whole tree has to close by brute force.

## Definition
A relaxation is weak when its optimum lies far below the integer optimum (large integrality gap) or when its optimal vertices are far from every integer point, so rounding fails and branching must dig deep. Weakness is caused by the formulation: loose big-M constants, redundant aggregations, symmetry, missing implied bounds — not by the LP algorithm used to solve it. Cuts, presolve and reformulation all attack weakness by replacing the relaxation with a tighter polyhedron. A large root gap is the clearest symptom and the cheapest quantity to measure.

## Why It Matters Here
- R13/Expected Solution explicitly names "weak LP relaxations" among the hard cases to demonstrate robustness on.
- Observed state: cuts are separated at the root only, one re-solve follows (src/milp/milp_solver.cpp:174-205), and `evidence/benchmarks/phase4.json` records cut-driven node reduction of **0.0%** — so a weak root relaxation is never tightened again inside the tree.
- Inference: with root-only cuts and four presolve rules, bound strength is essentially whatever the modeler wrote.

## Key Facts / Rules
- Integrality gap = (LP bound − IP optimum)/|IP optimum| for minimization; root gap is the number to report.
- Gap closure levers: presolve tightening, cutting planes, reformulation, stronger bounds propagation.
- Weakness also shows as primal difficulty: fractional vertices with no nearby integer point defeat rounding heuristics.
- Doubling tree size is the default cost of an unimproved weak relaxation — gap is paid in nodes, not in LP pivots.

## Related
- [[LP Relaxation]]
- [[Cut Validity]]
- [[Gomory Mixed Integer Cut]]
- [[Mixed Integer Rounding Cut]]
- [[Root-Only Cuts]]
- [[Marchand-1996-Mixed-Integer-Rounding]]

## Referenced By

- Research MOC
- [[Gomory Mixed Integer Cut|research/algorithms/Gomory Mixed Integer Cut]]
- [[Mixed Integer Rounding Cut|research/algorithms/Mixed Integer Rounding Cut]]
- [[Degeneracy|research/concepts/Degeneracy]]
- [[LP Relaxation|research/concepts/LP Relaxation]]
- [[Presolve|research/concepts/Presolve]]
- [[Root-Only Cuts|research/limitations/Root-Only Cuts]]
- research-dependency-map
- [[Cut Efficiency|research/metrics/Cut Efficiency]]
- [[Atamturk-2003-Cover-Inequalities-Mixed|research/papers/Atamturk-2003-Cover-Inequalities-Mixed]]
- [[Balas-1965-Additive-Algorithm-Solving|research/papers/Balas-1965-Additive-Algorithm-Solving]]
- [[Balas-1980-Cuts-Fixed-Rank|research/papers/Balas-1980-Cuts-Fixed-Rank]]
- [[Chung-2015-Computational-Study-Cutting|research/papers/Chung-2015-Computational-Study-Cutting]]
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy|research/papers/Chvatal-1973-Edmonds-Polytopes-Hierarchy]]
- [[Giallombardo-2025-Machine-Learning-Techniques|research/papers/Giallombardo-2025-Machine-Learning-Techniques]]
- [[Jabbar-2024-Cut-Based-Conflict-Analysis|research/papers/Jabbar-2024-Cut-Based-Conflict-Analysis]]
- [[Land-1960-Automatic-Method-Solving|research/papers/Land-1960-Automatic-Method-Solving]]
- [[Marchand-1996-Aggregation-Knapsack-Inequalities|research/papers/Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Padberg-1991-Branch-and-Cut-Algorithm|research/papers/Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Rex-0000-Pool-Not-Row|research/papers/Rex-0000-Pool-Not-Row]]
- [[Turner-2024-Potential-Cutting-Planes|research/papers/Turner-2024-Potential-Cutting-Planes]]
- [[Wang-2026-Enhancing-Presolve-Mixed|research/papers/Wang-2026-Enhancing-Presolve-Mixed]]
- [[Wolsey-1989-Strong-Formulations-Mixed|research/papers/Wolsey-1989-Strong-Formulations-Mixed]]