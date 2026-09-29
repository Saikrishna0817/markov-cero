---
type: paper
title: "Benchmarks for Optimization Software (Mittelmann)"
authors: "Mittelmann"
year: n.d.
venue: "Arizona State University (plato.asu.edu)"
doi: "(unverified)"
domain: [benchmark]
priority: ★
status: deep
tags: [paper, benchmark]
---

# Benchmarks for Optimization Software (Mittelmann)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The de-facto leaderboard for LP/MIP/conic/NLP solvers — standardized hardware, time limits and geometric means; the yardstick R16 ultimately points at.

## Metadata
| Field | Value |
|---|---|
| Authors | Mittelmann |
| Year | n.d. (living resource; consulted 2026) |
| Venue | Arizona State University, Decision Support Systems lab (plato.asu.edu/bench.html) |
| DOI/URL | http://plato.asu.edu/bench.html |

## Problem Addressed
Comparing optimization solvers is easy to do dishonestly: different hardware, different time limits, cherry-picked instances. The benchmark pages fix a protocol — published instance sets, fixed limits, declared machine/software configuration — so cross-solver comparisons are meaningful and repeatable.

## Core Contribution
- **Methodology:** Run many solvers (commercial and open) on standard sets (Netlib LP, MIPLIB MIP, conic/QP sets) under declared configurations; report per-instance times, solved counts and geometric means; refresh continuously.
- **Assumptions:** Solvers run with default or documented settings; instance sets are public; hardware disclosed.
- **Benchmarks/datasets:** Netlib LP, MIPLIB, Mittelmann's own LP/MILP sets, conic and NLP sets.
- **Metrics:** [[Geometric Mean Runtime]], solved-count within time limit, per-instance ratios; performance computed at matched accuracy.
- **Key results:** Live rankings in which commercial solvers lead and open solvers (HiGHS, SCIP, CBC) are measured in the same table — precisely the comparison our audit records as missing.

## Engineering-Relevant Knowledge
**Algorithms:** Benchmark protocol for LP/MIP solvers (accuracy-matched timing).
**Techniques:** Geometric means (not arithmetic) over instance ratios; fixed time limits with censoring disclosed; hardware/software disclosure block on every page.
**Implementation details:** To appear here we need: same instance files, same limit, same accuracy criterion, and a runner that records both our time and the reference solver's time — `benchmarks/runners` today records only our own.
**Equations/rules:** GM = exp( (1/|P|) Σ_p ln t_p ); ratio per instance then geometric mean across instances.
**Limitations/failure cases:** Leaderboards require running *other* solvers locally or citing their published numbers under matched setups; published numbers are only usable if hardware and limits match ours.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** The PS requires comparison against at least one established solver (R16) and Mittelmann-style sets (R15); this page defines both the instance sets and the reporting format expected of a serious submission.

## Evidence → Engineering Decision
- *Finding:* No comparison against HiGHS/CPLEX/Gurobi/SCIP exists anywhere in the repo (docs/audit/00-ground-truth.md, gap on R16) → *PS requirement:* R16 → *Component:* benchmarks/runners → *Metric:* [[Geometric Mean Runtime]] ratio vs. reference solver on matched instances
- *Finding:* No Mittelmann instance set has been ingested (R15 partial) → *PS requirement:* R15 → *Component:* src/io/mps.cpp → *Metric:* instances parsed and solved from published sets

## Related Papers
- [[Achterberg-2005-MIPLIB-2003]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]
- [[Mittelmann-n.d.-Mittelmann-LP-MILP]]
- [[Lodi-2013-Performance-Variability-Mixed]]

## Uses
- [[Mittelmann Benchmarks]]
