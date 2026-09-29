---
type: research-gap
tags: [research-gaps, benchmark]
status: stable
verified_on: 2026-09-25
---

# Mittelmann Coverage Gap

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> R15 names Mittelmann benchmark sets; the repo contains none of them — named deliverable, zero instances.

## Definition
The gap between the dataset requirement in R19/R15 and the actual instance inventory: no Mittelmann LP/MILP/QP instance set (as distributed on Mittelmann's pages) is present or referenced in the benchmark artifacts, so a named third of the benchmark obligation is unfulfilled. Coverage also implies *methodology*: Mittelmann-style results are aggregated (geometric means), run under uniform limits and compared to published leaderboards — a gap in set coverage is simultaneously a gap in that reporting style. Closing it is mostly logistical (fetch sets, wire runners, record limits/hardware) rather than algorithmic.

## Why It Matters Here
- R15 explicitly lists "MIPLIB, Netlib or Mittelmann" benchmark sets; R19 repeats Mittelmann in the Dataset Link text; PS-GAP-04 records its absence (docs/audit/00-ground-truth.md).
- Observed state: evidence/ contains netlib and miplib result files only; no Mittelmann instance sets found in the repo (audit C.3: "Mittelmann — no Mittelmann instance sets found → R15 partial").
- Inference: because R15 says "such as MIPLIB, Netlib **or** Mittelmann", the requirement may be satisfiable with two of three — but the PS names Mittelmann twice (description + dataset link), so evaluators may look for it specifically.

## Key Facts / Rules
- Sets live on Mittelmann's pages (LP, MILP, QP separately) with reference times — QP coverage matters because of R2.
- Report per-set geometric mean and per-instance table; record hardware and limits (closes reproducibility too).
- Instance selection should be pre-registered (which subsets, which limits) to avoid cherry-picking accusations.
- Cross-check: running the same sets enables comparison with published leaderboard values as an informal baseline.

## Related
- [[Mittelmann Benchmarks]]
- [[Geometric Mean Runtime]]
- [[Netlib LP Collection]]
- [[MIPLIB]]
- [[Mittelmann-n.d.-Mittelmann-LP-MILP]]

## Referenced By

- 21-traceability
- Algorithms MOC
- Datasets MOC
- Research MOC
- [[Mittelmann Benchmarks|research/datasets/Mittelmann Benchmarks]]
- cross-paper-synthesis
- research-dependency-map