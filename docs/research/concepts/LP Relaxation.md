---
type: concept
tags: [concepts, milp]
status: stable
verified_on: 2026-09-25
---

# LP Relaxation

> The continuous shadow of a MIP: every bound this solver proves, and every node it prunes, comes from here.

## Definition
The LP relaxation of a mixed-integer program is the model obtained by deleting integrality (and any combinatorial structure) while keeping the linear constraints, so the feasible set becomes a polyhedron containing every integer-feasible point. For minimization its optimum is a lower bound on the integer optimum, and the relaxation's optimal vertex is the point on which branching and cut separation act. Relaxing integrality is a one-bit change to the model; `sparse_canonicalize(model, relax_integrality=true)` performs exactly that (src/transform/sparse_canonicalize.cpp:119-121). Tighter formulations shrink the polyhedron toward the integer hull and improve the bound at the same cost of solving one LP.

## Why It Matters Here
- R5 requires branch-and-bound/branch-and-cut, all of which is "solve this relaxation, then divide it"; R13 asks about "difficult mixed-integer formulations", which is a statement about relaxation quality.
- Observed state: every node LP goes through `solve_node_relaxation` — quadratic models to [[QP-ADMM-Engine]], otherwise warm-started [[DualSimplexEngine]] or [[RevisedSimplexEngine]] (src/milp/node_lp.cpp:15-77).
- Observed state: root heuristics consume the relaxation point directly (`simple_rounding`, `feasibility_pump`, src/milp/milp_solver.cpp:146-163).

## Key Facts / Rules
- Minimization: LP optimum ≤ IP optimum; the difference normalized by |IP| is the integrality gap.
- Any valid cut or added bound tightens the relaxation; any branch splits it into two children whose union covers the parent.
- Solving a relaxation never needs integrality-aware code — the same certified LP engines serve all nodes.
- A relaxation can be *weak* (loose formulation) or *strong* (integer hull on the relevant face); strength is a property of the formulation, not the algorithm.

## Related
- [[Weak Relaxation]]
- [[Cut Validity]]
- [[Branch and Bound]]
- [[Rounding Heuristic]]
- [[Padberg-1991-Branch-and-Cut-Algorithm]]

## Referenced By

- [[Research MOC|research/Research MOC]]
- [[Branch and Bound|research/algorithms/Branch and Bound]]
- [[Feasibility Pump|research/algorithms/Feasibility Pump]]
- [[Cut Validity|research/concepts/Cut Validity]]
- [[Duality Gap|research/concepts/Duality Gap]]
- [[Weak Relaxation|research/concepts/Weak Relaxation]]
- [[Achterberg-0000-Objective-Feasibility-Pump|research/papers/Achterberg-0000-Objective-Feasibility-Pump]]
- [[Achterberg-2005-Branching-Rules-Revisited|research/papers/Achterberg-2005-Branching-Rules-Revisited]]
- [[Achterberg-2007-Constraint-Integer-Programming|research/papers/Achterberg-2007-Constraint-Integer-Programming]]
- [[Achterberg-2007-Improving-Feasibility-Pump|research/papers/Achterberg-2007-Improving-Feasibility-Pump]]
- [[Achterberg-2011-Rounding-Propagation-Heuristics|research/papers/Achterberg-2011-Rounding-Propagation-Heuristics]]
- [[Atamturk-2003-Cover-Inequalities-Mixed|research/papers/Atamturk-2003-Cover-Inequalities-Mixed]]
- [[Balas-1965-Additive-Algorithm-Solving|research/papers/Balas-1965-Additive-Algorithm-Solving]]
- [[Balas-1980-Cuts-Fixed-Rank|research/papers/Balas-1980-Cuts-Fixed-Rank]]
- [[Balas-1993-Lift-Project-Cutting|research/papers/Balas-1993-Lift-Project-Cutting]]
- [[Berthold-2006-Hybrid-Branching|research/papers/Berthold-2006-Hybrid-Branching]]
- [[Berthold-2007-RENS-Relaxation-Enforced|research/papers/Berthold-2007-RENS-Relaxation-Enforced]]
- [[Berthold-2025-Primal-Heuristics-Mixed|research/papers/Berthold-2025-Primal-Heuristics-Mixed]]
- [[Bixby-1994-Reduced-Cost-Fixing|research/papers/Bixby-1994-Reduced-Cost-Fixing]]
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy|research/papers/Chvatal-1973-Edmonds-Polytopes-Hierarchy]]
- [[Cornuejols-2008-Valid-Inequalities-Mixed|research/papers/Cornuejols-2008-Valid-Inequalities-Mixed]]
- [[Danna-2004-Exploring-Relaxation-Induced|research/papers/Danna-2004-Exploring-Relaxation-Induced]]
- [[Fischetti-2003-Local-Branching|research/papers/Fischetti-2003-Local-Branching]]
- [[Fischetti-2005-Feasibility-Pump|research/papers/Fischetti-2005-Feasibility-Pump]]
- [[Fischetti-2006-Feasibility-Pump-Heuristic|research/papers/Fischetti-2006-Feasibility-Pump-Heuristic]]
- [[Gamrath-2015-Progress-Presolving-Mixed|research/papers/Gamrath-2015-Progress-Presolving-Mixed]]
- [[Gomory-1958-Outline-Algorithm-Integer|research/papers/Gomory-1958-Outline-Algorithm-Integer]]
- [[Hillier-1969-Efficient-Heuristic-Procedures|research/papers/Hillier-1969-Efficient-Heuristic-Procedures]]
- [[Hoffman-1991-Improving-LP-Representations|research/papers/Hoffman-1991-Improving-LP-Representations]]
- [[Junger-1995-Branch-and-Cut-Combinatorial|research/papers/Junger-1995-Branch-and-Cut-Combinatorial]]
- [[Kumar-2010-Fifty-Years-Integer|research/papers/Kumar-2010-Fifty-Years-Integer]]
- [[Land-1960-Automatic-Method-Solving|research/papers/Land-1960-Automatic-Method-Solving]]
- [[Linderoth-2000-Impact-Branch-Bound|research/papers/Linderoth-2000-Impact-Branch-Bound]]
- [[Mahajan-2011-Presolving-Mixed-Integer-Linear|research/papers/Mahajan-2011-Presolving-Mixed-Integer-Linear]]
- [[Mexi-2026-Frank-Wolfe-based-Primal|research/papers/Mexi-2026-Frank-Wolfe-based-Primal]]
- [[Nemhauser-1988-Integer-Combinatorial-Optimization|research/papers/Nemhauser-1988-Integer-Combinatorial-Optimization]]
- [[Padberg-1991-Branch-and-Cut-Algorithm|research/papers/Padberg-1991-Branch-and-Cut-Algorithm]]
- [[Richard-2010-Group-Approach-Cutting|research/papers/Richard-2010-Group-Approach-Cutting]]
- [[Savelsbergh-1994-Preprocessing-Probing-Techniques|research/papers/Savelsbergh-1994-Preprocessing-Probing-Techniques]]
- [[Su-2025-Investigating-Exact-Effectiveness|research/papers/Su-2025-Investigating-Exact-Effectiveness]]
- [[Wolsey-1989-Strong-Formulations-Mixed|research/papers/Wolsey-1989-Strong-Formulations-Mixed]]
- [[Diving|research/techniques/Diving]]
- [[Rounding Heuristic|research/techniques/Rounding Heuristic]]