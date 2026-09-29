---
type: paper
title: "Benchmarking Optimization Software with Performance Profiles"
authors: "Dolan & Moré"
year: 2002
venue: "Mathematical Programming Computation"
doi: "(unverified)"
domain: [benchmark]
priority: ○
status: standard
tags: [paper, benchmark]
---
# Benchmarking Optimization Software with Performance Profiles

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> The correct way to plot comparative solver results: one curve per solver, robust to outliers, showing both efficiency and robustness.
## Metadata
| Field | Value |
|---|---|
| Authors | Dolan & Moré |
| Year | 2002 |
| Venue | Mathematical Programming Computation |
| DOI/URL | (unverified) |

## Problem Addressed
Tables of per-instance runtimes and arithmetic means are dominated by outliers and hide the shape of the comparison: a solver may win often but fail hard. The paper asks for a single plot that shows how often each solver is best and how badly it behaves when it is not.
## Core Contribution
- **Methodology:** For each problem p and solver s compute ratio r(p,s) = t(p,s) / min_s' t(p,s'); performance profile ρ(τ) = fraction of problems with r(p,s) ≤ τ; plot ρ versus τ for every solver.
- **Assumptions:** Times are positive; failed runs assigned a penalty time; same instance set and limits for all solvers.
- **Benchmarks/datasets:** Any solver comparison set (their examples: MINPACK/optimization problems).
- **Metrics:** Efficiency (ρ at τ=1), robustness (plateau height), and tail behaviour; expressed as a distribution rather than a mean.
- **Key results:** Profiles expose both "fastest often" and "never fails" properties in one figure; recommended standard for solver comparison papers (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Performance-profile computation over a solver×instance time matrix.
**Techniques:** Geometric-mean ratios as the scalar companion (inline rule: GM = exp(mean(ln r)); profile as the full distribution) — censored timeouts must be given an explicit penalty time.
**Implementation details:** A profile needs a reference solver's times alongside ours; `benchmarks/runners` currently produces only our times, so the matrix cannot be built yet.
**Limitations/failure cases:** Profiles need enough instances to be meaningful — 12 Netlib + 3 MIPLIB instances is too few for a defensible curve.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the presentation standard for R16 comparisons and immediately actionable once a second solver's timings exist.
## Evidence → Engineering Decision
- *Finding:* Only our own timings exist (evidence/netlib_results.csv, miplib_results.csv) with no competitor column → *PS requirement:* R16 → *Component:* benchmarks/runners → *Metric:* profile curves with [[Geometric Mean Runtime]] ratios
## Related Papers
- [[Mittelmann-n.d.-Benchmarks-Optimization-Software]]
- [[Lodi-2013-Performance-Variability-Mixed]]
## Uses
- [[Geometric Mean Runtime]]
