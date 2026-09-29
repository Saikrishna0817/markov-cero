---
type: engineering-decision
id: ED-006
status: accepted
date: 2026-09-25
tags: [engineering-decision, parallel, r7, p0, load-balancing]
---

# ED-006 — Load-Balanced Parallel Search or Demote the Claim

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Within this window, parallel tree search either gets a load-balanced (work-stealing) scheduler that re-measures ≥1× at 4 threads, or it is demoted to `--threads 1` with its speedup claim removed.

## Context (Observed fact)

- `evidence/benchmarks/phase4.json`: 4 threads → **0.56×** speedup, parallel efficiency **14%** — a measured regression, not a shortfall (09 R7 REGRESSION).
- `rg -i steal src/milp/` → 0 hits; workers pop from one mutex-protected shared best-bound heap (`include/markov_cero/milp/work_queue.hpp:18-57`) — a centralized queue with no rebalancing.
- `12` RW-2 is P0 and explicitly allows the cheap exit: "or default `--threads 1` until fixed"; 09 §6.3 lists the implied parallel-speedup claim as a REMOVE item because our own CSV contradicts it.

## Research Evidence

- [[Huangfu-2018-Parallelizing-Dual-Revised]] — decentralized work distribution for parallel revised simplex and tree work.
- [[Berthold-2019-Parallel-SCIP-UG]] — parallel SCIP: work stealing and incumbent handling patterns.
- [[Work Stealing]] — technique note for decentralized queue rebalancing.
- [[Negative Parallel Scaling]] — Lai & Sahni anomalies: parallelism can make B&B strictly worse, the exact observed shape.
- [[Deterministic Reduction]] — reproducible reduction so scaling curves stay benchmarkable.

## Decision

- **Option A (preferred):** work stealing with subtree granularity plus per-worker node/time budgets, then re-measure a 1/2/4/8-thread curve against the ED-001 baseline with hardware recorded and runs repeated.
- **Option B (automatic):** if Option A is not measurably positive by the deadline, ship `--threads 1` as the default, keep the parallel engine behind an explicit flag, and delete every parallel-speedup claim from README/STATUS/demo.
- There is no Option C: shipping the current design *with* a speedup claim stays forbidden.

## Consequences (positive / negative / neutral)

- **positive:** R7 becomes either a real number or an honest non-claim; removes the credibility trap (K2); the scaling curve becomes a demo asset.
- **negative:** Option A costs 1–2 days on the critical path; Option B forfeits a PS-visible feature.
- **neutral:** worker LP/basis handling and the TSAN CI job are unaffected either way.

## Alternatives Rejected

- *More threads on the current design* — amplifies contention and deepens the regression.
- *Full ParaSCIP-style rewrite* — complexity ≫ benefit at 8-thread scale (09 §6.4).
- *Keep claiming speedup while measuring 0.56×* — contradicted by our own evidence (12 §8.2).

## Linked Requirements

- R7, R18 → sih26119_problem_statement

## Related

- 09-research-code-alignment (R7, §6.2) · 12-keep-remove-rebuild (RW-2) · 13-restart-point (step 4) · 21-traceability §21.1 R7, §21.2
- ParallelTreeSearch · [[ED-001-comparison-harness-before-new-algorithms]]
