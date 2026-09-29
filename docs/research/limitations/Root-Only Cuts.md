---
type: limitation
tags: [limitations, milp]
status: stable
verified_on: 2026-09-25
---

# Root-Only Cuts

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Cuts are separated once at the root and never again — the tree inherits the root's gap for its entire life.

## Definition
Cut separation restricted to the root node means the LP relaxation is tightened a single time (a bounded number of rounds with a small cut budget) before branching begins, and every subsequent node solves a relaxation that only *loses* strength relative to its parent by branching, never gains. Modern branch-and-cut implementations separate at selected tree nodes as well, because deeper fractional points generate different (often still valuable) cuts, and because local cuts can be valid in a subtree even when globally invalid. The limitation is measurable: compare node counts and root gap with cuts disabled versus enabled.

## Why It Matters Here
- R5 requires "cutting planes" as a framework feature; delivering them only at the root satisfies the letter but not the mechanism.
- Observed state: root phase generates GMI+MIR cuts and re-solves once (src/milp/milp_solver.cpp:170-208); the tree loop contains no cut call; the parallel driver repeats the same root-only pattern (src/milp/parallel_tree_search.cpp:339-346).
- Observed state: `evidence/benchmarks/phase4.json` records cut-driven node reduction **0.0%** — Inference: as configured, the cut machinery changes no measurable search outcome.

## Key Facts / Rules
- Root cuts have the highest ROI (gap closes before tree growth) — root-only is a *baseline*, not a ceiling.
- In-tree cuts require: separation policy (which nodes, how often), local-validity tracking, warm-started re-solve.
- Cut budgets must be enforced per node; unbounded separation inflates LP cost faster than it prunes.
- Diagnosis: run with `--cuts` and `--no-cuts` and compare node count and root bound — that is the efficiency metric.

## Related
- [[Cut Validity]]
- [[Cut Efficiency]]
- [[Branch and Cut]]
- [[Weak Relaxation]]
- [[Mixed Integer Rounding Cut]]
- [[Turner-2024-Potential-Cutting-Planes]]

## Referenced By

- 21-traceability
- Algorithms MOC
- Architecture MOC
- Research MOC
- Research-Code Traceability MOC
- [[Branch and Cut|research/algorithms/Branch and Cut]]
- [[Weak Relaxation|research/concepts/Weak Relaxation]]
- [[ED-005-in-tree-cut-loop-not-more-cut-types|research/engineering-decisions/ED-005-in-tree-cut-loop-not-more-cut-types]]
- cross-paper-synthesis
- research-dependency-map
- [[Cut Efficiency|research/metrics/Cut Efficiency]]
- [[Cut Pooling|research/techniques/Cut Pooling]]