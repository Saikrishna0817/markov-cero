---
type: paper
title: "MPS format specification"
authors: "IBM/Gamst; Netlib lp/data docs"
year: "1970s"
venue: "IBM / Netlib documentation"
doi: "(unverified)"
domain: [io, lp]
priority: ○
status: standard
tags: [paper, io, lp]
---

# MPS format specification
> The interchange format every benchmark library ships in; parsing it correctly is the gate to all measurements.
## Metadata
| Field | Value |
|---|---|
| Authors | IBM/Gamst; Netlib lp/data docs |
| Year | 1970s– (era as listed) |
| Venue | IBM / Netlib documentation |
| DOI/URL | (unverified) |
## Problem Addressed
Benchmark instances (Netlib LP, MIPLIB) are distributed as free- or fixed-format MPS and, less often, LP files; a solver cannot run R15/R19 benchmarks without a reader that handles NAME/ROWS/COLUMNS/RHS/BOUNDS/RANGES/ENDATA, integer MARKER blocks and legacy dialect quirks.
## Core Contribution
- **Methodology:** Format specification plus Netlib documentation of field layout, section semantics and data types.
- **Assumptions:** Fixed-width columns in legacy files; `MARKER 'INTORG'`/`'INTEND'` delimits integer variable blocks; single-objective N rows.
- **Benchmarks/datasets:** Netlib `lp/data` and MIPLIB instance sets are themselves the deliverable this spec unlocks.
- **Metrics:** Parse success and round-trip fidelity of objective/constraint/bound values.
- **Key results:** A universal, solver-neutral exchange format that has carried every major benchmark suite for ~50 years (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Streaming MPS/LP reader, model builder, name-to-index mapping.
**Techniques:** Free/fixed field splitting, section-state machine, `INTORG`/`INTEND` marker tracking, RANGES row interpretation.
**Implementation details:** Lives in src/io/mps.cpp today; must preserve column order (deterministic basis), map row types N/E/L/G to constraints, reject duplicate names and report line-accurate errors instead of silently truncating.
**Equations/rules:** Row types N (free), E, L, G; bound types LO, UP, FR, MI, PL, BV, LI, UI; RANGES tightens a ≤/≥ row into a range row.
**Limitations/failure cases:** Dialect drift (comments, missing ENDATA, oversized fields), no support for model-level semantics — parsing is not validation.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R15/R19 require solving MIPLIB/Netlib/Mittelmann instances; every benchmark, external-solver comparison (R16) and robustness case (R17) enters the system through this reader, so silent parse errors invalidate all downstream evidence.
## Evidence → Engineering Decision
- *Finding:* All benchmark and comparison evidence flows through the MPS reader → *PS requirement:* R15 → *Component:* src/io/mps.cpp → *Metric:* benchmark instance solve-success rate
## Related Papers
- [[Hoffman-1991-Improving-LP-Representations]] [[Bixby-2002-Evolution-of-LP]] [[Maros-2003-Computational-Optimization-Techniques]]
## Uses
- [[Netlib LP Collection]] [[MIPLIB]]
