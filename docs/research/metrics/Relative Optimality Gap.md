---
type: metric
tags: [metrics, milp]
status: stable
verified_on: 2026-09-25
---

# Relative Optimality Gap

> (UB − LB)/|UB| — the only honest way to say "near-optimal" when the tree did not close.

## Definition
For a minimization problem with incumbent UB and best valid lower bound LB, the relative optimality gap is (UB − LB)/max(|UB|, ε) with ε a small guard against a zero objective; the absolute gap is UB − LB. The relative form normalizes across instances whose objectives differ by orders of magnitude, which is why termination tests and reported results use it. Any gap claim must state *both* bounds and where they came from: UB from the best feasible solution, LB from the strongest valid bound (LP-relaxation tree bound here). A gap of zero certifies optimality; a nonzero gap certifies only that no better solution was found within the search budget.

## Why It Matters Here
- R20 asks for optimal *or near-optimal* results on industrial instances — the gap is the required qualifier for every non-optimal run.
- Observed state: MILP defaults relative gap 1e-4, absolute gap 1e-6 (include/markov_cero/milp/milp_solver.hpp:16-31), used both for termination and for pruning (`best_upper_bound − absolute_gap_tolerance`, src/milp/milp_solver.cpp:287, 316).
- Observed state (risk): the driver reports "branch-and-cut MILP optimum" whenever an incumbent exists after termination (src/milp/milp_solver.cpp:462-470) — Inference: a node/time-limited run could be labelled optimal unless the bound genuinely proves it; the gap metric is the check on that claim.

## Key Facts / Rules
- rel_gap = (UB − LB)/max(|UB|, ε); report UB, LB, gap and the limit that stopped the search.
- Gap is a *bound-quality* metric — distinct from feasibility/KKT residuals which are *accuracy* metrics.
- Node/time limits truncate the gap; that truncation must be disclosed, not converted into "optimal".
- Pruning tolerance = absolute gap tolerance by default here; relative gap drives termination only.

## Related
- [[Duality Gap]]
- [[Branch and Bound]]
- [[KKT Residual]]
- [[MIPLIB]]
- [[Mittelmann Benchmarks]]

## Referenced By

- [[16-testing-evaluation-strategy|audit/16-testing-evaluation-strategy]]
- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Evaluation MOC|research/Evaluation MOC]]
- [[Research MOC|research/Research MOC]]
- [[Duality Gap|research/concepts/Duality Gap]]
- [[MIPLIB|research/datasets/MIPLIB]]
- [[QPLIB|research/datasets/QPLIB]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Achterberg-2005-General-Mixed-Integer|research/papers/Achterberg-2005-General-Mixed-Integer]]
- [[Achterberg-2005-MIPLIB-2003|research/papers/Achterberg-2005-MIPLIB-2003]]
- [[Achterberg-2007-Best-Estimate-Bound|research/papers/Achterberg-2007-Best-Estimate-Bound]]
- [[Achterberg-2007-Improving-Feasibility-Pump|research/papers/Achterberg-2007-Improving-Feasibility-Pump]]
- [[Anderssen-1984-NETLIB-LP-Test-Set|research/papers/Anderssen-1984-NETLIB-LP-Test-Set]]
- [[Applegate-2006-Traveling-Salesman-Problem|research/papers/Applegate-2006-Traveling-Salesman-Problem]]
- [[Balas-1996-Gomory-Cuts-Revisited|research/papers/Balas-1996-Gomory-Cuts-Revisited]]
- [[Berthold-2025-Primal-Heuristics-Mixed|research/papers/Berthold-2025-Primal-Heuristics-Mixed]]
- [[Bixby-1994-Reduced-Cost-Fixing|research/papers/Bixby-1994-Reduced-Cost-Fixing]]
- [[Chvatal-1973-Edmonds-Polytopes-Hierarchy|research/papers/Chvatal-1973-Edmonds-Polytopes-Hierarchy]]
- [[Clausen-1999-Branch-Bound-Algorithms|research/papers/Clausen-1999-Branch-Bound-Algorithms]]
- [[Danna-2004-Exploring-Relaxation-Induced|research/papers/Danna-2004-Exploring-Relaxation-Induced]]
- [[Fischetti-2003-Local-Branching|research/papers/Fischetti-2003-Local-Branching]]
- [[Fischetti-2005-Feasibility-Pump|research/papers/Fischetti-2005-Feasibility-Pump]]
- [[Fischetti-2015-Improving-Branch-Cut|research/papers/Fischetti-2015-Improving-Branch-Cut]]
- [[Gamrath-2015-Progress-Presolving-Mixed|research/papers/Gamrath-2015-Progress-Presolving-Mixed]]
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation|research/papers/Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Land-1960-Automatic-Method-Solving|research/papers/Land-1960-Automatic-Method-Solving]]
- [[Linderoth-2000-Impact-Branch-Bound|research/papers/Linderoth-2000-Impact-Branch-Bound]]
- [[Marchand-1996-Mixed-Integer-Rounding|research/papers/Marchand-1996-Mixed-Integer-Rounding]]
- [[Mexi-2026-Frank-Wolfe-based-Primal|research/papers/Mexi-2026-Frank-Wolfe-based-Primal]]
- [[Neveu-2016-Node-Selection-Strategies|research/papers/Neveu-2016-Node-Selection-Strategies]]
- [[Spoorendonk-2026-Presolve-Heuristics-HiGHS|research/papers/Spoorendonk-2026-Presolve-Heuristics-HiGHS]]
- [[Toth-2014-Vehicle-Routing-Problems|research/papers/Toth-2014-Vehicle-Routing-Problems]]
- [[Wang-2026-Enhancing-Presolve-Mixed|research/papers/Wang-2026-Enhancing-Presolve-Mixed]]
- [[Wolsey-1989-Strong-Formulations-Mixed|research/papers/Wolsey-1989-Strong-Formulations-Mixed]]
- [[Wright-1997-Primal-Dual-IPM|research/papers/Wright-1997-Primal-Dual-IPM]]