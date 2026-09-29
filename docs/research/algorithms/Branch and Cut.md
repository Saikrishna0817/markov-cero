---
type: concept
tags: [algorithms, milp]
status: stable
verified_on: 2026-09-25
---

# Branch and Cut

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Branch-and-bound with cutting planes separated at nodes — the standard framework, and where this solver currently stops at the root.

## Definition
Branch-and-cut interleaves cut separation with the branch-and-bound tree: at selected nodes (typically the root and sometimes throughout the tree) the LP relaxation is strengthened by valid inequalities, the node LP is re-solved with a warm start, and only then is branching performed. The combination matters because cuts shrink the bound early (cheap gap closure) while branching resolves what cuts cannot (fractionality with no separating hyperplane of bounded rank). Modern implementations also manage cut pools, local cuts valid only in a subtree, and per-node budgets so separation cost never exceeds its bound gain.

## Why It Matters Here
- R5 requires branch-and-cut and cutting planes as first-class features; R20's times depend on root gap closure.
- Observed state: driver order is root LP → heuristics → GMI+MIR cuts → one root re-solve → root gap check → root strong branching → best-bound tree search (src/milp/milp_solver.cpp:109-257); the parallel driver repeats the same root-only pattern (src/milp/parallel_tree_search.cpp:315-447).
- Observed state: cuts are **never separated inside the tree loop** (fact: no cut call in the node loop), which is tracked as [[Root-Only Cuts]] with 0.0% cut-driven node reduction in evidence/benchmarks/phase4.json.

## Key Facts / Rules
- Cut cycle policy: separate → re-solve with warm start → limit rounds (here `max_cut_rounds 5`, ≤10 cuts per family) → branch.
- Cuts must satisfy [[Cut Validity]] in original space after presolve/scaling maps are undone.
- Local vs global validity: cuts proven under node bounds belong to that subtree only.
- Root cuts have outsized value: most gap closure happens before the first branch.

## Related
- [[Branch and Bound]]
- [[Cut Validity]]
- [[Gomory Mixed Integer Cut]]
- [[Strong Branching]]
- BranchAndCut
- [[Padberg-1991-Branch-and-Cut-Algorithm]]

## Referenced By

- 21-traceability
- BranchAndCut
- Algorithms MOC
- Architecture MOC
- Research MOC
- Research-Code Traceability MOC
- [[Branch and Bound|research/algorithms/Branch and Bound]]
- [[MIPLIB|research/datasets/MIPLIB]]
- [[Root-Only Cuts|research/limitations/Root-Only Cuts]]
- cross-paper-synthesis
- [[Achterberg-2005-General-Mixed-Integer|research/papers/Achterberg-2005-General-Mixed-Integer]]
- [[Applegate-2006-Traveling-Salesman-Problem|research/papers/Applegate-2006-Traveling-Salesman-Problem]]
- [[Atamturk-2008-Integer-Programming-Software|research/papers/Atamturk-2008-Integer-Programming-Software]]
- [[Balas-1996-Gomory-Cuts-Revisited|research/papers/Balas-1996-Gomory-Cuts-Revisited]]
- [[Berthold-2007-Heuristics-Branch-Cut|research/papers/Berthold-2007-Heuristics-Branch-Cut]]
- [[Bixby-2020-Compiling-Mixed-Integer|research/papers/Bixby-2020-Compiling-Mixed-Integer]]
- [[COIN-OR-n.d.-CBC-Solver-Documentation|research/papers/COIN-OR-n.d.-CBC-Solver-Documentation]]
- [[Chung-2015-Computational-Study-Cutting|research/papers/Chung-2015-Computational-Study-Cutting]]
- [[Cornuejols-2001-Branch-and-Cut-Algorithms|research/papers/Cornuejols-2001-Branch-and-Cut-Algorithms]]
- [[Fischetti-2003-Local-Branching|research/papers/Fischetti-2003-Local-Branching]]
- [[Fischetti-2015-Improving-Branch-Cut|research/papers/Fischetti-2015-Improving-Branch-Cut]]
- [[Giallombardo-2025-Machine-Learning-Techniques|research/papers/Giallombardo-2025-Machine-Learning-Techniques]]
- [[Gomory-1958-Outline-Algorithm-Integer|research/papers/Gomory-1958-Outline-Algorithm-Integer]]
- [[Jabbar-2024-Cut-Based-Conflict-Analysis|research/papers/Jabbar-2024-Cut-Based-Conflict-Analysis]]
- [[Junger-1995-Branch-and-Cut-Combinatorial|research/papers/Junger-1995-Branch-and-Cut-Combinatorial]]
- [[Kallrath-2002-Planning-Scheduling-Industry|research/papers/Kallrath-2002-Planning-Scheduling-Industry]]
- [[Linan-2025-Trends-Perspectives-Deterministic|research/papers/Linan-2025-Trends-Perspectives-Deterministic]]
- [[Linderoth-2005-Noncommercial-Software-Mixed|research/papers/Linderoth-2005-Noncommercial-Software-Mixed]]
- [[Padberg-1991-Branch-and-Cut-Algorithm|research/papers/Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Padberg-2005-Classical-Cuts-Mixed|research/papers/Padberg-2005-Classical-Cuts-Mixed]]
- [[Su-2025-Investigating-Exact-Effectiveness|research/papers/Su-2025-Investigating-Exact-Effectiveness]]
- [[Toth-2014-Vehicle-Routing-Problems|research/papers/Toth-2014-Vehicle-Routing-Problems]]
- [[Turner-2024-Potential-Cutting-Planes|research/papers/Turner-2024-Potential-Cutting-Planes]]
- [[Cut Pooling|research/techniques/Cut Pooling]]