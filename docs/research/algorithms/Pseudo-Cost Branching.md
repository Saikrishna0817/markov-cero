---
type: concept
tags: [algorithms, milp]
status: stable
verified_on: 2026-09-25
---

# Pseudo-Cost Branching

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Remember how much each variable has moved the bound in each direction — branching by accumulated experience instead of by trial.

## Definition
A pseudo-cost records, per variable and per direction, the observed objective-bound change divided by the unit change of the branching constraint (e.g. from x_j ≤ v to x_j ≤ v−1). When a variable is branched on for the first time it has no history, so it is probed (one strong-branching round) or assigned an average/default cost; thereafter the score is the sum of down- and up-costs, optionally weighted. After children are solved, the actual bound deltas update the records — making the rule self-calibrating as the tree progresses. Reliability branching formalizes when the accumulated count is "enough" to stop probing.

## Why It Matters Here
- R5 (advanced node selection strategies) and R20 (node count ⇒ runtime): pseudo-costs are the default production rule and the repo's default choice.
- Observed state: `branching_strategy pseudo_cost` is the default; child bound deltas update shared pseudo-costs (src/milp/milp_solver.cpp:320-330); the parallel tree exposes `SharedPseudoCosts` guarded by a mutex (src/milp/parallel_tree_search.cpp:70-73).
- Observed state: strong branching at the root is what *feeds* these records (src/milp/milp_solver.cpp:228-257) — Inference: without root probing, early in-tree pseudo-costs would be unseeded defaults.

## Key Facts / Rules
- cost_j = (bound change) / (constraint change) maintained separately for down and up directions.
- Score(candidate) = down_cost_j · |Δ_down| + up_cost_j · |Δ_up| (unit changes for pure bound branching).
- Cold start problem: unseen variables need strong-branch probes (reliability threshold, e.g. count ≥ η before trusting).
- Update only from *solved* children; pruning before solution yields no reliable delta.

## Related
- [[Strong Branching]]
- [[Branch and Bound]]
- [[Diving]]
- BranchAndCut
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Driebeek-1966-Algorithm-Assignment-Problem]]

## Referenced By

- BranchAndCut
- Algorithms MOC
- Architecture MOC
- Research MOC
- [[Branch and Bound|research/algorithms/Branch and Bound]]
- [[Strong Branching|research/algorithms/Strong Branching]]
- [[ED-009-defer-ml-branching|research/engineering-decisions/ED-009-defer-ml-branching]]
- cross-paper-synthesis
- [[Achterberg-2005-Branching-Rules-Revisited|research/papers/Achterberg-2005-Branching-Rules-Revisited]]
- [[Achterberg-2007-Best-Estimate-Bound|research/papers/Achterberg-2007-Best-Estimate-Bound]]
- [[Berthold-2006-Hybrid-Branching|research/papers/Berthold-2006-Hybrid-Branching]]
- [[Berthold-2013-Cloud-Branching|research/papers/Berthold-2013-Cloud-Branching]]
- [[Driebeek-1966-Algorithm-Assignment-Problem|research/papers/Driebeek-1966-Algorithm-Assignment-Problem]]
- [[Held-2006-Lookahead-Branching-Mixed|research/papers/Held-2006-Lookahead-Branching-Mixed]]
- [[Hollenbeck-2014-Important-Branching-Decisions|research/papers/Hollenbeck-2014-Important-Branching-Decisions]]
- [[Neveu-2016-Node-Selection-Strategies|research/papers/Neveu-2016-Node-Selection-Strategies]]
- [[Rader-0000-Selection-Variables-MIP|research/papers/Rader-0000-Selection-Variables-MIP]]
- [[Schweizer-0000-Restart-Strategies-MIP|research/papers/Schweizer-0000-Restart-Strategies-MIP]]
- [[Turner-0000-Intelligent-Branching-Large|research/papers/Turner-0000-Intelligent-Branching-Large]]
- [[Zhang-2025-Learning-Select-Nodes|research/papers/Zhang-2025-Learning-Select-Nodes]]
- [[Diving|research/techniques/Diving]]