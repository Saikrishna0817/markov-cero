---
type: paper
title: "A Algorithm for Testing the Adequacy of Algorithms / Decision Tree for Optimization Software"
authors: "Rice; Mittelmann"
year: n.d.
venue: "(unverified)"
doi: "(unverified)"
domain: [benchmark]
priority: ○
status: standard
tags: [paper, benchmark]
---
# A Algorithm for Testing the Adequacy of Algorithms / Decision Tree for Optimization Software

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Two pieces of benchmark tooling: a formal framework for deciding whether an algorithm is adequate for a problem class, and a decision-tree service that routes users to suitable optimization software.
## Metadata
| Field | Value |
|---|---|
| Authors | Rice; Mittelmann (no year stated in source list) |
| Year | n.d. |
| Venue | (unverified) |
| DOI/URL | (unverified) |

## Problem Addressed
"A is faster than B" is meaningless without specifying the problem class, the performance criterion and the test set. The entries address formal evaluation design (what evidence makes an algorithm adequate) and the practical question of how a user finds software fitting their problem characteristics.
## Core Contribution
- **Methodology:** Structured algorithm-testing framework (define problem classes, criteria such as time/accuracy/reliability, and decision rules); a decision-tree tool mapping problem features to candidate optimization software.
- **Assumptions:** Test problems are representative of the target class; criteria are declared before running.
- **Benchmarks/datasets:** Curated test problems tied to the decision classes (not re-verified).
- **Metrics:** Adequacy criteria: time, accuracy, reliability across the class rather than single-instance wins.
- **Key results:** Evaluation must be class-based and pre-registered in its criteria; tooling (decision tree) operationalizes software selection (details not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Algorithm evaluation methodology; decision-tree classification of problem characteristics.
**Techniques:** Pre-declare criteria and test classes; separate "adequate" (meets threshold) from "best" (relative ranking).
**Implementation details:** Informs how we structure the qualification gate: declare instance classes (LP/MILP/QP, size bands) and pass/fail thresholds before running, instead of narrating results afterwards.
**Limitations/failure cases:** Framework-level guidance only; no instance data or solver numbers to reuse.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R20's benchmark goal ("consistently deliver optimal or near-optimal") is an adequacy statement; this gives the methodological framing for writing that gate honestly.
## Evidence → Engineering Decision
- *Finding:* Pass/fail thresholds are currently per-instance ad-hoc (`pass` column in evidence CSVs) → *PS requirement:* R20 → *Component:* benchmarks/runners → *Metric:* declared class-level adequacy thresholds met
## Related Papers
- [[Mittelmann-n.d.-Benchmarks-Optimization-Software]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]
## Uses
- [[Geometric Mean Runtime]]
