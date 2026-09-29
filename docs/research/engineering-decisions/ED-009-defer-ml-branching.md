---
type: engineering-decision
id: ED-009
status: accepted
date: 2026-09-25
tags: [engineering-decision, deferred, branching, r5, r18, p3]
---

# ED-009 — Defer ML Branching to the Roadmap

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Machine-learning branching and cut selection are explicitly out of scope for this cycle: classical reliability branching is what R5 asks for, and the README's Phase 7 ML plan is delisted.

## Context (Observed fact)

- 09 §6.3 records "ML-assisted branching (Phase 7 plan, README:43)" with verdict **DEFER**; §6.4 lists it first among ideas technically unsuitable for this project now.
- No training corpus, no data pipeline, no trained model and no evaluation harness exist in the repo, and the idea-submission deadline is 2026-09-30 (constraint C5, `00-ground-truth`).
- R5's PS clause names B&B, B&C, cuts, presolve, heuristics and node selection — ML appears nowhere in the problem statement's "Explicitly NOT required" list either way, and is never required.
- The implemented branching stack is classical and measured: pseudo-cost default with strong branching to seed it (`src/milp/milp_solver.cpp:228-257, 360-388`).

## Research Evidence

- [[Zhang-2025-Learning-Select-Nodes]] — learned node selection needs labelled runs and an eval harness.
- [[Giallombardo-2025-Machine-Learning-Techniques]] — ML for MIP carries training-corpus and evaluation overhead beyond a 5-day window.
- [[Turner-0000-Intelligent-Branching-Large]] — intelligent branching is still validated against classical rules; no transfer without measurement.
- [[Pseudo-Cost Branching]] — the classical mechanism already implemented and sufficient for R5.
- [[Achterberg-2005-Branching-Rules-Revisited]] — reliability branching with screened candidates: the evidence-backed default we ship instead.

## Decision

- ML branching / ML cut selection: **deferred** — roadmap item P3 only, after the deadline.
- Remove the Phase 7 ML claim from README:43 and from any demo slide so docs match evidence (RW-10, evaluator risk K2).
- The R5 answer is classical reliability branching: pseudo-cost with strong-branching probes, documented with the parameters we actually use.
- The research notes stay in the vault as a roadmap corpus — deferral is scoping, not deletion.

## Consequences (positive / negative / neutral)

- **positive:** focus on the six P0 items; no unevaluatable feature in the demo; doc drift removed.
- **negative:** a differentiating-looking feature is descoped; the corpus investment pays off only later.
- **neutral:** no code changes — README/STATUS text and roadmap wording only.

## Alternatives Rejected

- *Ship an untrained or heuristic "ML" model* — unevaluable, credibility risk in front of judges.
- *Partial pipeline (data collection only)* — consumes days and proves nothing by the deadline.
- *Keep the README claim* — doc-vs-evidence contradiction, the exact K2 trap 12 §8.2 removes.

## Linked Requirements

- R5, R18 → sih26119_problem_statement

## Related

- 09-research-code-alignment (§6.3, §6.4) · 12-keep-remove-rebuild (RW-10) · 13-restart-point (intentionally not sequenced) · 21-traceability §21.3 M15–M16
- BranchAndCut · [[ED-005-in-tree-cut-loop-not-more-cut-types]]
