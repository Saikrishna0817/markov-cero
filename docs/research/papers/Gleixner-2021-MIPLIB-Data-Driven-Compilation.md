---
type: paper
title: "MIPLIB 2017: Data-Driven Compilation of the 6th MIP Library"
authors: "Gleixner, Hendel, Gamrath, Achterberg et al."
year: 2021
venue: "Mathematical Programming Computation"
doi: "10.1007/s12532-020-00194-3"
domain: [benchmark]
priority: ★
status: deep
tags: [paper, benchmark]
---

# MIPLIB 2017: Data-Driven Compilation of the 6th MIP Library

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Builds the standard MIP benchmark library as a *data-driven* selection: a 1065-instance collection plus a 240-instance benchmark set chosen by features, clustering and expert scoring — with verified best-known solutions.

## Metadata
| Field | Value |
|---|---|
| Authors | Gleixner, Hendel, Gamrath, Achterberg et al. |
| Year | 2021 |
| Venue | Mathematical Programming Computation |
| DOI/URL | 10.1007/s12532-020-00194-3 |

## Problem Addressed
Old MIPLIB releases were compiled by ad-hoc submission, leaving gaps in coverage, duplicate structures and unverified solutions. The paper addresses how to construct a benchmark library that is representative of real MIP use, statistically defensible, and accompanied by trustworthy reference solutions.

## Core Contribution
- **Methodology:** Gather a large candidate pool, compute instance features, cluster to find duplicates/outliers, score with expert and automated criteria, then split into a broad collection (1065) and a benchmark set (240); verify every best-known solution computationally.
- **Assumptions:** Features capture hardness-relevant structure; verified solutions are required for honest gap reporting.
- **Benchmarks/datasets:** MIPLIB 2017 itself: 1065 collection + 240 benchmark instances.
- **Metrics:** Feature-space coverage, instance diversity, verification status of reference solutions, observed solver performance distributions.
- **Key results:** A benchmark set that is small enough to run routinely yet representative; instances not solvable to proven optimality are annotated with gaps rather than silently mislabeled.

## Engineering-Relevant Knowledge
**Algorithms:** Instance feature computation, clustering-based deduplication, benchmark curation.
**Techniques:** Data-driven selection instead of expert-only curation; verified reference solutions as the basis for [[Relative Optimality Gap]] reporting.
**Implementation details:** We have `evidence/miplib_results.csv` with only STEIN9, STEIN15 and FLUGPL — tiny collection members, not the 240 benchmark set; `src/io/mps.cpp` must handle the library's free-form MPS variants at scale.
**Equations/rules:** Gap% = |f_ours − f_bks| / max(1, |f_bks|) against the library's best-known solution; only instances with verified references may appear in published tables.
**Limitations/failure cases:** Running the benchmark set at all requires time limits, deterministic settings and multiple runs — none of which our current harness enforces.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R15/R19 name MIPLIB explicitly as the benchmark; the 240-instance set is the concrete definition of "solved MIPLIB", and the verification requirement matches our zero-trust verifier philosophy.

## Evidence → Engineering Decision
- *Finding:* Only 3 MIPLIB instances have ever been run (evidence/miplib_results.csv) → *PS requirement:* R15 → *Component:* benchmarks/runners → *Metric:* instances solved of 240 with [[Relative Optimality Gap]]
- *Finding:* No best-known-solution database is stored in-repo → *PS requirement:* R16 → *Component:* src/verify/primal_verifier.cpp → *Metric:* verification pass rate vs. MIPLIB reference solutions

## Related Papers
- [[Achterberg-2005-MIPLIB-2003]]
- [[Koch-n.d.-MIPLIB-Design-Experiments]]
- [[Lodi-2013-Performance-Variability-Mixed]]
- [[Dolan-2002-Benchmarking-Optimization-Software]]

## Uses
- [[MIPLIB]]
- [[Geometric Mean Runtime]]
