---
type: paper
title: "Presolving Mixed-Integer Linear Programs"
authors: "Mahajan"
year: 2011
venue: "Encyclopedia of Operations Research"
doi: "10.1002/9780470400531.eorms0437"
domain: [presolve]
priority: ○
status: standard
tags: [paper, presolve]
---
# Presolving Mixed-Integer Linear Programs

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Short survey + bibliography of MILP presolve — an efficient map of the literature.
## Metadata
| Field | Value |
|---|---|
| Authors | Mahajan |
| Year | 2011 |
| Venue | Encyclopedia of Operations Research |
| DOI/URL | 10.1002/9780470400531.eorms0437 |
## Problem Addressed
MILP presolve spans decades of results across many codes; newcomers need a compact orientation before reading primary papers (Savelsbergh, Andersen, Achterberg). The encyclopedia entry organizes the rule families and points to sources.
## Core Contribution
- **Methodology:** Survey grouping rules: singleton/doubleton, bound-based fixing, probing/logical, aggregation, infeasibility/redundancy detection; notes on implementation concerns.
- **Assumptions:** General MILP input; reader has LP background.
- **Benchmarks/datasets:** Illustrative examples only.
- **Metrics:** Qualitative.
- **Key results:** Catalogue + bibliography (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** MILP presolve families overview.
**Techniques:** Term definitions and rule taxonomy used consistently in later papers.
**Implementation details:** Use it to structure presolve documentation/tests for our growing rule set (each rule needs: precondition, transformation record, postsolve inverse, test).
**Limitations/failure cases:** Encyclopedia brevity — no algorithms in enough detail to implement directly.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R5 + R18 (transparent, extensible foundation): a survey gives vocabulary for documenting each presolve rule we add and its evidence trail.
## Evidence → Engineering Decision
- *Finding:* Rule families can be enumerated and tested independently → *PS requirement:* R5 → *Component:* tests/presolve_test.cpp → *Metric:* per-rule reduction counters (PresolveStatistics fields).
## Related Papers
- [[Brearley-1975-Analysis-Mathematical-Programming]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]
## Uses
- [[Presolve]]
- [[LP Relaxation]]
