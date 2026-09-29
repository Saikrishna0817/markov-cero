---
type: paper
title: "The Impact of Branch-and-Bound Procedures on the Complexity of Integer Programming"
authors: "Linderoth & Savelsbergh"
year: 2000
venue: "(not stated in list)"
doi: "(unverified)"
domain: [branching]
priority: ★
status: deep
tags: [paper, branching]
---

# The Impact of Branch-and-Bound Procedures on the Complexity of Integer Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Disentangles node count from LP cost: the procedure that minimizes nodes is often not the one that minimizes time.

## Metadata
| Field | Value |
|---|---|
| Authors | Linderoth & Savelsbergh |
| Year | 2000 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
Branch-and-bound has many knobs (node selection, branching rule, cut policy, diving). Which of them actually determine computational complexity, and how do they interact — do fewer nodes mean less time?

## Core Contribution
- **Methodology:** Instrument branch-and-bound to vary node selection and branching procedures systematically, separating tree-size effects from per-node LP cost effects.
- **Assumptions:** Controlled implementation with matched settings; standard MILP instances.
- **Benchmarks/datasets:** Test instances of the era (not itemized in list).
- **Metrics:** Node counts, LP solves/time, total time — reported separately.
- **Key results:** Procedure choice changes node counts by orders of magnitude while LP cost per node moves differently — i.e., **optimize the pair, not node count alone**.

## Engineering-Relevant Knowledge
**Algorithms:** Node-selection and branching interactions within B&B.
**Techniques:** Reporting nodes *and* LP time separately (methodological requirement for any of our benchmark tables).
**Implementation details:** Our only end-to-end metric today is wall time; `src/milp/milp_solver.cpp` should emit node counts, LP time share and probe counts per configuration so branching changes can be attributed (exactly this paper's protocol).
**Equations/rules:** Total time ≈ Σ_nodes (LP time + cut time + heuristic time + bookkeeping) — decompose before drawing conclusions.
**Limitations/failure cases:** Older engine/instance mix; conclusions on scaling need re-checking on modern MIPLIB.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It prescribes the measurement discipline for R16 (comparison against another solver) and explains why our 0.0% node-reduction figure must be paired with per-node cost data.

## Evidence → Engineering Decision
- *Finding:* node count and runtime can move in opposite directions → *PS requirement:* R16, R20 → *Component:* src/milp/milp_solver.cpp (instrumentation) → *Metric:* node count + LP time share, [[Geometric Mean Runtime]]

## Related Papers
- [[Achterberg-2005-Branching-Rules-Revisited]]
- [[Held-2006-Lookahead-Branching-Mixed]]
- [[Hollenbeck-2014-Important-Branching-Decisions]]
- [[Achterberg-2007-Best-Estimate-Bound]]

## Uses
- [[Branch and Bound]] [[Relative Optimality Gap]] [[Geometric Mean Runtime]] [[LP Relaxation]]
