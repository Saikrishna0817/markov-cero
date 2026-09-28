---
type: paper
title: "MIPLIB 2003"
authors: "Achterberg, Koch & Martin"
year: 2005
venue: "Operations Research Letters"
doi: "(unverified)"
domain: [benchmark]
priority: ★
status: deep
tags: [paper, benchmark]
---

# MIPLIB 2003

> The reference instance taxonomy and performance-measurement methodology for mixed-integer programming — the predecessor of MIPLIB 2017 and the origin of most modern MIP benchmarking practice.

## Metadata
| Field | Value |
|---|---|
| Authors | Achterberg, Koch & Martin |
| Year | 2005 |
| Venue | Operations Research Letters (as listed) |
| DOI/URL | (unverified) |

## Problem Addressed
Before MIPLIB 2003, MIP solvers were compared on anecdotal instance collections with inconsistent metrics, so "solver A is faster" was unfalsifiable. The paper addresses building a curated instance library and defining how MIP performance should be measured and reported.

## Core Contribution
- **Methodology:** Curate a large instance collection (~1,000 models; exact count (approximate)), classify instances by structural type, and lay out measurement practice: fixed time limits, counts of solved instances, treatment of unknown optimality, and comparison across solvers on identical hardware/settings.
- **Assumptions:** Solvers must run under identical limits and settings; instances need trusted best-known solutions.
- **Benchmarks/datasets:** MIPLIB 2003 (approximately 1,000 instances).
- **Metrics:** Number/percentage of instances solved to proven optimality within a limit; runtime distributions; quality of incumbents where proof fails.
- **Key results:** Established the reporting conventions still used by solver papers (solved-counts under a time limit, verified references) (specific numbers not re-verified here).

## Engineering-Relevant Knowledge
**Algorithms:** MIP performance measurement; instance classification/taxonomy.
**Techniques:** Report solved counts under fixed limits; distinguish proven optima from best-found values; compare only under matched hardware and settings.
**Implementation details:** Our evidence CSVs report `pass`, `verified`, `nodes_explored`, `runtime_ms` per instance but no time limit and no cross-solver column — the missing piece for R16.
**Equations/rules:** Solved% = (# proven optimal within T) / N; never average runtimes of solved instances only without disclosing censored (timeout) runs.
**Limitations/failure cases:** A single solved-count hides variability (see Lodi–Tramontani) and instance mix; taxonomy-based subsets must be reported separately.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It defines what "we benchmarked MIPLIB" legally means for our submission — verified references, matched settings, explicit time limits — which our current 3-instance run does not satisfy.

## Evidence → Engineering Decision
- *Finding:* evidence/miplib_results.csv covers 3 collection instances, no time limit column, no reference-solver column → *PS requirement:* R16 → *Component:* benchmarks/runners → *Metric:* solved% under a fixed time limit vs. an established solver

## Related Papers
- [[Gleixner-2021-MIPLIB-Data-Driven-Compilation]]
- [[Lodi-2013-Performance-Variability-Mixed]]
- [[Mittelmann-n.d.-Benchmarks-Optimization-Software]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]

## Uses
- [[MIPLIB]]
- [[Relative Optimality Gap]]
