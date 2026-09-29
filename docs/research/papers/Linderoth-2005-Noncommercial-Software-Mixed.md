---
type: paper
title: "Noncommercial Software for Mixed-Integer Linear Programming (CBC)"
authors: "Linderoth & Ralphs"
year: 2005
venue: "(not listed in source)"
doi: "(unverified)"
domain: [milp]
priority: ✦
status: standard
tags: [paper, milp]
---
# Noncommercial Software for Mixed-Integer Linear Programming (CBC)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Architectural description of CBC: class hierarchy, component customization, open-source design decisions.
## Metadata
| Field | Value |
|---|---|
| Authors | Linderoth & Ralphs |
| Year | 2005 |
| Venue | (not listed in source) |
| DOI/URL | (unverified) — optimization-online.org/2004/12/945/ |
## Problem Addressed
CBC (COIN-OR Branch and Cut) proved a noncommercial MIP solver could be competitive, but its value to a from-scratch team is architectural: how CbcModel, cut generators, heuristics and branching objects are layered and overridden.
## Core Contribution
- **Methodology:** Description of CBC's object structure: model owns tree state; cut generators/heuristics/branching rules are pluggable classes; LP layer swappable (CLP); hooks for customization without forking.
- **Assumptions:** C++ OOP; LP solver behind an interface; single-node processing as the extension unit.
- **Benchmarks/datasets:** MIP benchmark experience of the era (qualitative).
- **Metrics:** Solve performance + engineering flexibility.
- **Key results:** Open solver within striking distance of commercial codes on many classes (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-and-cut with pluggable components.
**Techniques:** Interface segregation for cuts/heuristics/branching; solver layering.
**Implementation details:** Directly comparable to `src/milp/` layout; note R10: we may study CBC's architecture but must not build on it. Also shows how to expose component toggles — already mirrored by our CLI flags (`--cuts`, `--heuristics`, `--branching`).
**Limitations/failure cases:** Documented design is 2005-era; CBC later gained cuts/heuristics we lack (and vice versa: our GPU path).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R1 (core, not modeling env), R10 (from scratch — read for design ideas only), R18 (extensible). Use as a design review reference for component interfaces.
## Evidence → Engineering Decision
- *Finding:* Component toggles enable fair ablation/benchmarking → *PS requirement:* R16 → *Component:* apps/markov_cero_solve.cpp (CLI flags) → *Metric:* per-flag performance deltas in evidence reports.
## Related Papers
- [[Bixby-2020-Compiling-Mixed-Integer]]
- [[COIN-OR-n.d.-CBC-Solver-Documentation]]
## Uses
- [[Branch and Cut]]
- [[Branch and Bound]]
