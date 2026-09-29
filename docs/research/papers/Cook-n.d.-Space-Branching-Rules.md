---
type: paper
title: "The Space of Branching Rules for Branch-and-Cut"
authors: "Cook"
year: "n.d."
venue: "(not listed in source)"
doi: "(unverified)"
domain: [milp]
priority: ○
status: standard
tags: [paper, milp]
---
# The Space of Branching Rules for Branch-and-Cut

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Design space of branching rules: dimensions along which rules vary and how to compare them.
## Metadata
| Field | Value |
|---|---|
| Authors | Cook |
| Year | n.d. (year not given in source list) |
| Venue | (not listed in source) |
| DOI/URL | (unverified) |
## Problem Addressed
Branching rules are usually presented individually (most fractional, strong, pseudo-cost), making systematic comparison hard. The work frames the *space* of rules — candidate selection, score function, direction choice — so new rules can be located and evaluated.
## Core Contribution
- **Methodology:** Taxonomy of branching decisions: which variable candidates to consider, how to score them (objective change, infeasibility, inference), which direction to pick; discussion of evaluation criteria (nodes vs. time).
- **Assumptions:** Branch-and-cut framework with an LP relaxation per node; score computable from trial information.
- **Benchmarks/datasets:** Comparative MIP experiments (as reported in the source).
- **Metrics:** Node count, solve time, reliability of scores.
- **Key results:** No single rule dominates; screening + scoring choices interact with LP cost (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branching-rule design dimensions.
**Techniques:** Candidate screening; score normalization; direction symmetry.
**Implementation details:** Our implemented rules: most-fractional, pseudo-cost, strong, reliability (`--branching` in STATUS.md; `src/milp/branch_selector.cpp`, `src/milp/strong_branching.cpp` with pseudo-costs) — already covers four points in this design space; this paper's taxonomy is the map for the Phase-7 ML ranker (STATUS.md roadmap).
**Limitations/failure cases:** Score-based rules can mislead on degenerate relaxations ([[Degeneracy]]); evaluation must separate node count from time (LP cost varies per rule).
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R5 (advanced node selection/branching) — provides the vocabulary to document and extend our branching interface and to benchmark rules fairly (R16).
## Evidence → Engineering Decision
- *Finding:* Branching quality must be measured in time, not nodes alone → *PS requirement:* R5, R16 → *Component:* src/milp/branch_selector.cpp + src/milp/strong_branching.cpp → *Metric:* solve time and nodes under `--branching` variants.
## Related Papers
- [[Achterberg-2005-General-Mixed-Integer]]
- [[Padberg-1991-Branch-and-Cut-Algorithm]]
## Uses
- [[Strong Branching]]
