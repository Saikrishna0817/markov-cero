---
type: paper
title: "Cut-Based Conflict Analysis in MIP"
authors: "Jabbar, Gleixner et al."
year: 2024
venue: "MPC"
doi: "(unverified)"
domain: [branching, cuts]
priority: ○
status: standard
tags: [paper, branching]
---

# Cut-Based Conflict Analysis in MIP

> Learns from infeasible nodes: turn the reason a sub-tree failed into a cut (conflict/implié inequality) that prunes every other node containing that reason.

## Metadata
| Field | Value |
|---|---|
| Authors | Jabbar, Gleixner et al. |
| Year | 2024 |
| Venue | Mathematical Programming Computation (list: "MPC") |
| DOI/URL | (unverified) |

## Problem Addressed
When a node proves infeasible, standard MIP discards everything learned there. SAT solvers extract a clause (conflict) explaining *why*; MIP historically did not, so the same failure is rediscovered repeatedly.

## Core Contribution
- **Methodology:** Backtrack the reason for infeasibility over bound/implication decisions and cut-based inferences, extract a conflict inequality (a row over bound decisions), and reuse it as a cut or as domain reduction elsewhere in the tree.
- **Assumptions:** Ability to trace inferences (bounds, cuts) back to their origins; conflict store with bound management.
- **Benchmarks/datasets:** Hard MIPLIB instances (MPC 2024).
- **Metrics:** Nodes, time, conflicts learned/reused.
- **Key results:** Brings SAT-style learning into MIP — significant gains where infeasible exploration dominates.

## Engineering-Relevant Knowledge
**Algorithms:** Conflict analysis / no-good extraction over a reason graph; cut-based conflict inequalities.
**Techniques:** Reason tracking for bound changes and cut implications; periodic conflict-store cleanup.
**Implementation details:** Needs provenance tracking on every bound change in `src/milp/node_lp.cpp` and cuts in `cut_pool.cpp` — a substantial feature we do not have. Correctly categorized as future/advanced work.
**Equations/rules:** Conflict inequality: Σ_{j∈S} (bound-decision literals) ≥ 1 style no-good, added like a cut.
**Limitations/failure cases:** Bookkeeping overhead on every inference; conflicts can be huge; invalid if reason tracking is even slightly wrong.

## Applicability to Our Project
**Classification:** Not applicable
**Why:** High value but high complexity and out of the initial R5 minimum (cuts + heuristics + node selection already listed); revisit only after root/tree cuts and heuristics are demonstrably working.

## Evidence → Engineering Decision
- *Finding:* we discard all information from infeasible nodes → *PS requirement:* R5 → *Component:* src/milp/node_lp.cpp (future reason tracking) → *Metric:* node count on hard instances

## Related Papers
- [[Turner-2024-Potential-Cutting-Planes]]
- [[Schweizer-0000-Restart-Strategies-MIP]]
- [[Achterberg-2007-Best-Estimate-Bound]]
- [[Padberg-2005-Classical-Cuts-Mixed]]

## Uses
- [[Branch and Cut]] [[Cut Validity]] [[Cut Pooling]] [[Weak Relaxation]]
