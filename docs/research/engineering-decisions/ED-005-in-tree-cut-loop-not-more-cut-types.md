---
type: engineering-decision
id: ED-005
status: accepted
date: 2026-09-25
tags: [engineering-decision, cuts, branch-and-cut, r5, p0]
---

# ED-005 — In-Tree Cut Loop, Not More Cut Types

> Fix *where* cuts are separated before adding *which* cuts: re-separate inside the tree under budgets with a bounded pool; new cut families come only after the measurement exists.

## Context (Observed fact)

- Cuts are generated only at the root: `src/milp/milp_solver.cpp:165-208` (GMI + MIR, one re-solve), the same root-only pattern repeated in `src/milp/parallel_tree_search.cpp:339-346`. No separation inside the tree loop.
- Measured effect: `evidence/benchmarks/phase4.json` cut-node reduction **0.0%** — R5 is PARTIAL in practice despite a complete-looking skeleton.
- `include/markov_cero/milp/cut_pool.hpp` already provides `Cut`, `filter_cuts` (efficacy sort, parallelism rejection, `max_cuts = 10`) and `add_cuts_to_model` — the pool exists but is used only for the root round.
- `12` RW-1 is P0 with a target of >20% node reduction on a MIPLIB set; 09 §6.2 puts in-tree regeneration at P0 and cover/clique/odds families at P2.

## Research Evidence

- [[Cornuejols-2001-Branch-and-Cut-Algorithms]] — branch-and-cut is separation *during* the search.
- [[Cut Efficiency]] — cuts are measured by what they prune, not by how many are generated.
- [[Cut Pooling]] — a bounded, deduplicated pool makes repeated separation affordable.
- [[Cut Validity]] — every re-separated cut must be re-checked against the node relaxation.
- [[Root-Only Cuts]] — limitation note documenting the current state and its 0.0% number.

## Decision

- Wire separation into the node loop: separation frequency, per-node and per-round cut budgets, validity/violation re-checks where the cut is added, and pool reuse so a cut is not regenerated at every node.
- The feature is "done" only when `evidence/benchmarks/cut_reduction.csv` shows >20% node reduction against the ED-001 baseline on the shared instance set.
- Add cover/clique/odds families (RW, P2) **after** that number exists — otherwise new generators are unmeasurable and root-only placement still yields ~0%.

## Consequences (positive / negative / neutral)

- **positive:** closes the "R5 unmet in practice" finding with a number; reuses existing generator and pool code; gives the harness a second knob to report.
- **negative:** cuts add rows and destroy sparsity, so budgets are mandatory (cross-paper §5); more LP re-solves per node; bookkeeping complexity.
- **neutral:** GMI/MIR generator internals are unchanged — only placement and lifetime change.

## Alternatives Rejected

- *Add more cut families first* — they would still be root-only; 0.0% stays ~0.0%.
- *More root rounds* — diminishing returns; does not touch in-tree pruning.
- *Remove cuts, or separate without budgets* — loses a named PS algorithm, or pays more than it prunes (cross-paper §5).

## Linked Requirements

- R5, R20 → [[sih26119_problem_statement]]

## Related

- [[09-research-code-alignment]] (R5, §6.2) · [[12-keep-remove-rebuild]] (RW-1) · [[13-restart-point]] (step 3) · [[21-traceability]] §21.1 R5, §21.2
- [[CutGenerators]] · [[BranchAndCut]] · [[ED-010-presolve-depth-over-new-engine]]
