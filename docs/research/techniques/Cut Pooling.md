---
type: technique
tags: [technique, cuts]
status: stable
verified_on: 2026-09-25
---

# Cut Pooling

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Store generated inequalities once, filter by validity/duplicate detection, reuse across nodes.

## Definition
A cut pool keeps generated cuts (GMI, MIR, cover, …) in a shared structure with activity
tracking and (optionally) duplicate/validity checks, so cuts generated at one node can be
reused at later nodes instead of being discarded.

## Why It Matters Here
PS R5 requires branch-and-cut with cutting planes; observed state is that
markov-cero generates GMI/MIR cuts **at the root only** (see [[Root-Only Cuts]] and
`src/milp/cut_pool.cpp`), so the pool exists but is never fed after root. Without pooling +
re-generation, cut-node reduction measures 0.0% (`evidence/benchmarks/phase4.json:39`).

## Key Facts / Rules
- Typical pool: row-wise storage + dual activity vector; global cut count bounded to control LP size.
- Cuts must be re-validated at nodes where bounds changed (Inference: standard practice).

## Related
- [[Gomory Mixed Integer Cut]]
- [[Mixed Integer Rounding Cut]]
- [[Branch and Cut]]
- [[Cut Validity]]

## Referenced By

- 15-roadmap
- 21-traceability
- Architecture MOC
- Research-Code Traceability MOC
- [[ED-005-in-tree-cut-loop-not-more-cut-types|research/engineering-decisions/ED-005-in-tree-cut-loop-not-more-cut-types]]
- [[Balas-1993-Lift-Project-Cutting|research/papers/Balas-1993-Lift-Project-Cutting]]
- [[Balas-1996-Gomory-Cuts-Revisited|research/papers/Balas-1996-Gomory-Cuts-Revisited]]
- [[Benichou-1997-Linear-Programming-Implementations|research/papers/Benichou-1997-Linear-Programming-Implementations]]
- [[Giallombardo-2025-Machine-Learning-Techniques|research/papers/Giallombardo-2025-Machine-Learning-Techniques]]
- [[Jabbar-2024-Cut-Based-Conflict-Analysis|research/papers/Jabbar-2024-Cut-Based-Conflict-Analysis]]
- [[Marchand-1996-Aggregation-Knapsack-Inequalities|research/papers/Marchand-1996-Aggregation-Knapsack-Inequalities]]
- [[Padberg-2005-Classical-Cuts-Mixed|research/papers/Padberg-2005-Classical-Cuts-Mixed]]
- [[Rex-0000-Pool-Not-Row|research/papers/Rex-0000-Pool-Not-Row]]
- [[Turner-0000-Intelligent-Branching-Large|research/papers/Turner-0000-Intelligent-Branching-Large]]
- [[Turner-2024-Potential-Cutting-Planes|research/papers/Turner-2024-Potential-Cutting-Planes]]