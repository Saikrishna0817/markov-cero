---
type: paper
title: "mipfeas Benchmark"
authors: "Bussieck & Dirkse"
year: 2026
venue: "GAMS (benchmark release)"
doi: "(unverified)"
domain: [benchmark]
priority: ✦
status: standard
tags: [paper, benchmark]
---
# mipfeas Benchmark

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> A 233-instance set designed specifically to evaluate primal heuristics — finding feasible solutions fast — rather than proof performance.
## Metadata
| Field | Value |
|---|---|
| Authors | Bussieck & Dirkse |
| Year | 2026 |
| Venue | GAMS (https://www.gams.com/mipfeas/) |
| DOI/URL | https://www.gams.com/mipfeas/ |

## Problem Addressed
MIP benchmark sets measure optimality/proof, so a solver with weak heuristics can look fine. This benchmark isolates the primal side: on instances where feasibility itself is hard, how quickly does each solver produce a valid solution?
## Core Contribution
- **Methodology:** Curate instances where finding *any* feasible solution is the challenge; run solvers under fixed settings and record time-to-first-feasible and success rate.
- **Assumptions:** Feasibility is verifiable independently; instance difficulty is dominated by primal search, not proof.
- **Benchmarks/datasets:** 233 instances (per source list).
- **Metrics:** Success rate finding a feasible solution, time to first feasible solution, primal integral style measures.
- **Key results:** A dedicated view in which heuristic quality separates solvers that aggregate benchmarks hide (specific rankings not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Primal heuristic evaluation (rounding, diving, FP, RINS-type methods).
**Techniques:** Time-to-first-feasible as the primary metric; independent feasibility verification of every candidate.
**Implementation details:** Our heuristics (`src/milp/heuristics.cpp`, feasibility pump/rounding paths) are exercised only inside full solves — no time-to-first-feasible telemetry exists in evidence CSVs.
**Limitations/failure cases:** A feasibility-focused set does not measure cutting/branching quality; use it alongside MIPLIB, not instead.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R5 requires heuristics; without a primal-focused set we cannot show they work, only that the solver sometimes reaches optimality.
## Evidence → Engineering Decision
- *Finding:* evidence/miplib_results.csv records heuristics_found counts but no time-to-first-feasible → *PS requirement:* R5 → *Component:* src/milp/heuristics.cpp → *Metric:* time to first feasible solution / success rate
## Related Papers
- [[Fischetti-2015-Improving-Branch-Cut]]
- [[Lodi-2013-Performance-Variability-Mixed]]
## Uses
- [[Warm Start]]
