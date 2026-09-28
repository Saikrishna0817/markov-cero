---
type: concept
tags: [algorithms, milp]
status: stable
verified_on: 2026-09-25
---

# Strong Branching

> Probe a handful of candidates by actually solving their children — expensive, accurate, and the standard way to seed pseudo-costs.

## Definition
Strong branching temporarily branches on each candidate variable, solves the resulting child LP relaxations (one or two probes per child), scores the candidate by the bound improvement observed, and discards the trial constraints before committing to the chosen variable. It buys reliable scoring information at the price of several LP solves per node, so production solvers apply it only at the root or on a small candidate set, and use "reliability branching" to stop probing once accumulated pseudo-costs become trustworthy. The trial solves warm-start from the parent basis and are thrown away — no tree growth occurs during probing.

## Why It Matters Here
- R5 names advanced node selection; branching quality drives node count more than any other single decision (Linderoth & Savelsbergh 2000).
- Observed state: root strong branching runs after cuts to seed pseudo-costs and tighten bounds (src/milp/milp_solver.cpp:228-257); in-tree strong branching is used only when requested and a basis exists (src/milp/milp_solver.cpp:360-388); the parallel driver copies root results into shared pseudo-costs under a lock (src/milp/parallel_tree_search.cpp:406-410).
- Observed state: default `branching_strategy` is `pseudo_cost`, with strong branching as the seeding mechanism (include/markov_cero/milp/milp_solver.hpp:16-31).

## Key Facts / Rules
- Score = weighted down/up bound improvement from trial LPs; probing costs ≈ 2 LP solves per candidate per probe round.
- Reliability branching (Achterberg, Koch & Martin 2005): probe until a candidate's pseudo-cost estimate is "reliable", then stop.
- Always warm-start probes from the parent basis; discard trial rows after scoring.
- Root probing has outsized value because it initializes every later pseudo-cost update.

## Related
- [[Pseudo-Cost Branching]]
- [[Branch and Bound]]
- [[Warm Start]]
- [[BranchAndCut]]
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Held-2006-Lookahead-Branching-Mixed]]

## Referenced By

- [[BranchAndCut|codebase/components/BranchAndCut]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Branch and Cut|research/algorithms/Branch and Cut]]
- [[Pseudo-Cost Branching|research/algorithms/Pseudo-Cost Branching]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Achterberg-2005-Branching-Rules-Revisited|research/papers/Achterberg-2005-Branching-Rules-Revisited]]
- [[Balas-1993-Lift-Project-Cutting|research/papers/Balas-1993-Lift-Project-Cutting]]
- [[Berthold-2006-Hybrid-Branching|research/papers/Berthold-2006-Hybrid-Branching]]
- [[Berthold-2013-Cloud-Branching|research/papers/Berthold-2013-Cloud-Branching]]
- [[Cook-n.d.-Space-Branching-Rules|research/papers/Cook-n.d.-Space-Branching-Rules]]
- [[Driebeek-1966-Algorithm-Assignment-Problem|research/papers/Driebeek-1966-Algorithm-Assignment-Problem]]
- [[Held-2006-Lookahead-Branching-Mixed|research/papers/Held-2006-Lookahead-Branching-Mixed]]
- [[Hollenbeck-2014-Important-Branching-Decisions|research/papers/Hollenbeck-2014-Important-Branching-Decisions]]
- [[Maros-2003-Generalized-Dual-Phase|research/papers/Maros-2003-Generalized-Dual-Phase]]
- [[Rader-0000-Selection-Variables-MIP|research/papers/Rader-0000-Selection-Variables-MIP]]
- [[Turner-0000-Intelligent-Branching-Large|research/papers/Turner-0000-Intelligent-Branching-Large]]