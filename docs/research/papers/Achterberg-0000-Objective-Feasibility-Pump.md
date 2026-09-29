---
type: paper
title: "Objective Feasibility Pump / crossover heuristics"
authors: "Achterberg & Berthold; Berthold"
year: 0000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [heuristics]
priority: ○
status: standard
tags: [paper, heuristics]
---

# Objective Feasibility Pump / crossover heuristics

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Cross-reference row: the objective FP lives in #112 and the crossover/dive heuristics in #115 — recorded so the module list has no orphan entries.

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg & Berthold; Berthold (as in list) |
| Year | **not stated in reference list** — slug uses 0000 placeholder (see manifest) |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified); list marks this row "(Covered within #112/#115)" |

## Problem Addressed
No standalone publication is identified for this row; it exists to record two features — objective-aware feasibility pumping and crossover-style heuristics — that the reference list expects the reader to find inside #112 and #115.

## Core Contribution
- **Methodology:** Pointer entry. Objective FP = weighted projection toward good objective values (#112); crossover heuristics = transferring a fractional/IPM point to a feasible integer point, discussed with the SCIP heuristic taxonomy (#115).
- **Assumptions:** n/a (no primary source to extract assumptions from).
- **Benchmarks/datasets:** n/a — see #112 (mean gap 55% → 29.5%) and #115.
- **Metrics:** n/a — see the primary notes.
- **Key results:** None attributable here; **do not cite this row as a source.**

## Engineering-Relevant Knowledge
**Algorithms:** [[Feasibility Pump]] with objective term; crossover heuristics (IPM solution → integer feasible point).
**Techniques:** Objective-weighted projection; dive-to-crossover handoff.
**Implementation details:** We have FP without an objective term; crossover is relevant to R4 (interior-point + crossover), which the audit flags as **not implemented**.
**Equations/rules:** none (pointer row).
**Limitations/failure cases:** Bibliographically incomplete — year, venue and titles all missing; treat as an index card, not a citation.

## Applicability to Our Project
**Classification:** Background knowledge
**Why:** Useful only as an index to #112/#115 and as a reminder that crossover heuristics (tied to the unmet R4 crossover requirement) are expected somewhere in the design.

## Evidence → Engineering Decision
- *Finding:* crossover heuristics referenced but IPM/crossover absent from code → *PS requirement:* R4 → *Component:* src/lp/first_order/pdlp.cpp (no crossover path today) → *Metric:* basis extraction feasibility ([[Basis]])

## Related Papers
- [[Achterberg-2007-Improving-Feasibility-Pump]]
- [[Berthold-2007-Heuristics-Branch-Cut]]
- [[Fischetti-2005-Feasibility-Pump]]
- [[Berthold-2025-Primal-Heuristics-Mixed]]

## Uses
- [[Feasibility Pump]] [[Diving]] [[Basis]] [[LP Relaxation]]
