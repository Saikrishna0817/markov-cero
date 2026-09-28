---
type: concept
tags: [algorithms, milp]
status: stable
verified_on: 2026-09-25
---

# Branch and Bound

> Solve relaxations, split on fractional variables, prune with bounds — the completeness argument behind every MILP result.

## Definition
Branch-and-bound searches a binary tree over the integer variables: each node is a subproblem defined by the LP relaxation plus the accumulated branching bounds, and the node LP's objective gives a lower bound (minimization) for that entire subtree. The algorithm keeps the global incumbent (best feasible solution found) and prunes any node whose bound meets or exceeds the incumbent minus tolerance, whose relaxation is infeasible, or whose integrality is satisfied. Correctness comes from the fact that children's feasible sets exactly cover the parent's integer points, so pruning never discards a better integer solution. Node selection (best bound, best estimate, plunge) and branching variable selection determine the tree size far more than the per-node LP cost does.

## Why It Matters Here
- R5 names branch-and-bound first among required algorithms; R20's industrial times are dominated by node counts.
- Observed state: single-threaded driver at src/milp/milp_solver.cpp:21 with defaults `max_nodes 50000`, 60s limit, relative gap 1e-4, `node_strategy best_bound_plunge`, `branching_strategy pseudo_cost`; tree is a `std::priority_queue` with a best-bound comparator (src/milp/milp_solver.cpp:261-316).
- Observed state: `Options::node_strategy` is declared but never read — Inference: only best-bound ordering is actually in effect (open question in docs/codebase/components/BranchAndCut.md).

## Key Facts / Rules
- Node bound ≥ incumbent − tolerance ⇒ prune; infeasible relaxation ⇒ prune; all-integer feasible ⇒ update incumbent.
- Bound propagation: improving the incumbent immediately tightens pruning for every open node.
- Correctness requires children to partition the parent's integer-feasible region (x_j ≤ ⌊v⌋ vs x_j ≥ ⌊v⌋+1).
- Node limit / time limit termination must report the *remaining* gap honestly — not "optimal".

## Related
- [[LP Relaxation]]
- [[Warm Start]]
- [[Branch and Cut]]
- [[Pseudo-Cost Branching]]
- [[BranchAndCut]]
- [[Land-1960-Automatic-Method-Solving]]

## Referenced By

- [[BranchAndCut|codebase/components/BranchAndCut]]
- [[ParallelTreeSearch|codebase/components/ParallelTreeSearch]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Research-Code Traceability MOC|research/Research-Code Traceability MOC]]
- [[Branch and Cut|research/algorithms/Branch and Cut]]
- [[Pseudo-Cost Branching|research/algorithms/Pseudo-Cost Branching]]
- [[Strong Branching|research/algorithms/Strong Branching]]
- [[Duality Gap|research/concepts/Duality Gap]]
- [[LP Relaxation|research/concepts/LP Relaxation]]
- [[Warm Start|research/concepts/Warm Start]]
- [[Relative Optimality Gap|research/metrics/Relative Optimality Gap]]
- [[Achterberg-2005-Branching-Rules-Revisited|research/papers/Achterberg-2005-Branching-Rules-Revisited]]
- [[Achterberg-2007-Best-Estimate-Bound|research/papers/Achterberg-2007-Best-Estimate-Bound]]
- [[Balas-1965-Additive-Algorithm-Solving|research/papers/Balas-1965-Additive-Algorithm-Solving]]
- [[Beach-2022-Compact-Mixed-Integer|research/papers/Beach-2022-Compact-Mixed-Integer]]
- [[Beach-2024-Enhancements-Discretization-Approaches|research/papers/Beach-2024-Enhancements-Discretization-Approaches]]
- [[Berthold-2006-Hybrid-Branching|research/papers/Berthold-2006-Hybrid-Branching]]
- [[Berthold-2007-RENS-Relaxation-Enforced|research/papers/Berthold-2007-RENS-Relaxation-Enforced]]
- [[Berthold-2019-Parallel-SCIP-UG|research/papers/Berthold-2019-Parallel-SCIP-UG]]
- [[Carpentier-1962-Origin-Economic-Dispatch|research/papers/Carpentier-1962-Origin-Economic-Dispatch]]
- [[Clausen-1999-Branch-Bound-Algorithms|research/papers/Clausen-1999-Branch-Bound-Algorithms]]
- [[Danna-2004-Exploring-Relaxation-Induced|research/papers/Danna-2004-Exploring-Relaxation-Induced]]
- [[Driebeek-1966-Algorithm-Assignment-Problem|research/papers/Driebeek-1966-Algorithm-Assignment-Problem]]
- [[Eckstein-1994-Control-Strategies-Parallel|research/papers/Eckstein-1994-Control-Strategies-Parallel]]
- [[Held-2006-Lookahead-Branching-Mixed|research/papers/Held-2006-Lookahead-Branching-Mixed]]
- [[Hollenbeck-2014-Important-Branching-Decisions|research/papers/Hollenbeck-2014-Important-Branching-Decisions]]
- [[Lai-1984-Anomalies-Parallel-Branch|research/papers/Lai-1984-Anomalies-Parallel-Branch]]
- [[Land-1960-Automatic-Method-Solving|research/papers/Land-1960-Automatic-Method-Solving]]
- [[Linderoth-2000-Impact-Branch-Bound|research/papers/Linderoth-2000-Impact-Branch-Bound]]
- [[Linderoth-2005-Noncommercial-Software-Mixed|research/papers/Linderoth-2005-Noncommercial-Software-Mixed]]
- [[Neveu-2016-Node-Selection-Strategies|research/papers/Neveu-2016-Node-Selection-Strategies]]
- [[Pochet-2006-Production-Planning-Mixed|research/papers/Pochet-2006-Production-Planning-Mixed]]
- [[Rader-0000-Selection-Variables-MIP|research/papers/Rader-0000-Selection-Variables-MIP]]
- [[Schweizer-0000-Restart-Strategies-MIP|research/papers/Schweizer-0000-Restart-Strategies-MIP]]
- [[Schweizer-2018-Deterministic-Parallel-MIP|research/papers/Schweizer-2018-Deterministic-Parallel-MIP]]
- [[Su-2025-Investigating-Exact-Effectiveness|research/papers/Su-2025-Investigating-Exact-Effectiveness]]
- [[Turner-0000-Intelligent-Branching-Large|research/papers/Turner-0000-Intelligent-Branching-Large]]
- [[Zhang-2025-Learning-Select-Nodes|research/papers/Zhang-2025-Learning-Select-Nodes]]
- [[Zhang-n.d.-On-Design-Parallel-Branch|research/papers/Zhang-n.d.-On-Design-Parallel-Branch]]
- [[Work Stealing|research/techniques/Work Stealing]]