---
type: codebase-tech-debt
tags: [codebase, technical-debt, cuts, performance]
severity: medium
status: resolved
verified_on: 2026-09-25
evidence:
  - "src/milp/search_separate_cuts.cpp:3-130"
  - "evidence/cut_effectiveness.csv"
---

# Root-Only Cuts

> **Resolved (RW-1).** Gomory and MIR cutting planes are now separated in-tree on a
> frequency-gated loop with pool deduplication; measured node reduction is 39.0%
> (STEIN9) and 45.4% (STEIN15) against the `--no-cuts` baseline.

## Resolution
- In-tree separation block lives at `src/milp/search_separate_cuts.cpp:3` ("In-tree cut
  separation: frequency-gated, pool-deduped, bounded rounds (ED-005)"):
  gated by `options.enable_cuts`, `options.separation_frequency`, and
  `node_lp_res.basis.has_value()`; bounded by `options.max_cut_rounds`.
- Cuts are stored per node (`BranchNode::local_cuts`), inherited by both children,
  applied to the node LP model, deduplicated against the root cut list and the
  inherited pool via `compute_cosine_similarity` (threshold 0.95), and trimmed to
  `options.max_pool_cuts` by violation.
- A node re-solve that fails after cut addition reverts the fresh round, so
  unverified cuts never propagate to children.
- Measurement: `evidence/cut_effectiveness.csv` — STEIN9 41→25 nodes (39.0%),
  STEIN15 273→149 nodes (45.4%), FLUGPL 1107→1799 nodes (cuts cost more than they
  save there; all runs Optimal + verified + reference-matching).
  Gate: ≥20% reduction on ≥2/3 instances — PASS.

## Historical Observations (pre-RW-1)
- Cut generation originally only at root (`milp_solver.cpp:165` comment), measured
  `cut_node_reduction: 0.0%` in `evidence/benchmarks/phase4.json:39,49`.
- Parallel engine still runs root cuts only until the WorkerContext-owned
  `process_node` call site adopts `solve_parallel_node_with_cuts`. The helper in
  `parallel_tree_search_root_cuts.cpp` uses a private model per worker,
  frequency gating, local-cut inheritance through `work_queue.cpp`, and
  reverts an unverified round. Root cut exceptions are reported to stderr and
  the root model is kept unchanged when the cut LP does not certify.

## Related
- [[negative-parallel-scaling]] · [[testing-gaps]] · [[blend-numerical-failure]]
