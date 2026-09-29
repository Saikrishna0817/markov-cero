---
type: engineering-decision
id: ED-001
status: accepted
date: 2026-09-25
tags: [engineering-decision, evaluation, r16, p0, baseline]
---

# ED-001 — Comparison Harness Before New Algorithms

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Build the external-solver comparison harness first; every later algorithmic claim is measured against it.

## Context (Observed fact)

- Zero comparison artifacts repo-wide: searching `evidence/`, `reports/`, `benchmarks/` for highs/cplex/gurobi/cbc/scip → 0 hits (09 R16).
- Six phases shipped with single-solver CSVs only (`evidence/netlib_results.csv`, `evidence/miplib_results.csv`) — no baseline, no geometric means.
- Every other P0 item (in-tree cuts, parallel scaling, GPU re-run) needs a fixed measuring instrument before its fix can be shown to help (experiments E1–E6, 09 §6.6).
- `13-restart-point` names this the restart point: step 1 of 8, ~1 day, unblocked by anything.

## Research Evidence

- [[Dolan-2002-Benchmarking-Optimization-Software]] — performance profiles over solver×instance ratios; the format an evaluator recognises.
- [[Geometric Mean Runtime]] — geometric mean of ratios with penalty time for timeouts; arithmetic means rejected by the corpus.
- [[No External Baseline]] — limitation note: nothing in the repo compares to anyone.
- [[Missing External Baseline Comparison]] — research-gap note; R16 is binary (exists / does not).
- [[Lodi-2013-Performance-Variability-Mixed]] — single-run tables are not evidence, so the harness must support repeated runs.

## Decision

- First build item: `scripts/run_compare.py --baseline highs` over a **pre-registered** shared instance set (Netlib + MIPLIB), emitting markdown + CSV with status, objective, time, gap per instance, geometric-mean ratios and a Dolan–Moré profile.
- HiGHS runs as an **external process only** — never linked, never vendored — so `scripts/check-sovereignty.py` and constraint C1 stay untouched (comparison ≠ building upon, 00 §A.6).
- Record hardware (CPU/GPU/RAM) into `evidence/hardware.md` in the same step (closes PS-GAP-05).
- No new algorithm work merges before the harness exists; every fix lands with before/after rows.

## Consequences (positive / negative / neutral)

- **positive:** closes the only hard PS gap that is 100% missing; makes RW-1/RW-2/RW-9 measurable; restores credibility of performance claims (K1/K2).
- **negative:** ~1 day up front; baseline version, timeout and hardware must be pinned and reported.
- **neutral:** no solver-core code changes; the sovereignty guard is unaffected.

## Alternatives Rejected

- *Algorithm-first* — reproduces the Phase 0 finding: implementation ahead of evidence.
- *Self-baseline (old vs new markov-cero)* — not an "established solver"; fails R16 wording.
- *Commercial baseline* — license friction; PS explicitly allows open-source, and picking instances after seeing results violates pre-registration methodology.

## Linked Requirements

- R16, R20 (and the R15 presentation format) → sih26119_problem_statement

## Related

- 09-research-code-alignment · 12-keep-remove-rebuild (RW-3) · 13-restart-point (step 1) · 21-traceability §21.2
- [[ED-007-honest-gpu-scoping]] (reuses the harness) · [[ED-008-retain-zero-trust-verifiers]] (certificate columns in the table)
