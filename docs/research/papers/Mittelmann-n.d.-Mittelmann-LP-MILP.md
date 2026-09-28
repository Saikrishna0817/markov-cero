---
type: paper
title: "Mittelmann LP/MILP Benchmark Sets"
authors: "Mittelmann"
year: n.d.
venue: "Arizona State University (plato.asu.edu/ftp/lpfree.html)"
doi: "(unverified)"
domain: [benchmark]
priority: ✦
status: standard
tags: [paper, benchmark]
---
# Mittelmann LP/MILP Benchmark Sets
> The free instance sets behind the Mittelmann leaderboards (Netlib/Kennington LP collections and MILP sets) — the raw material for R15/R19 benchmark runs.
## Metadata
| Field | Value |
|---|---|
| Authors | Mittelmann |
| Year | n.d. (living resource; consulted 2026) |
| Venue | Arizona State University — plato.asu.edu/ftp/lpfree.html |
| DOI/URL | https://plato.asu.edu/ftp/lpfree.html |

## Problem Addressed
Public leaderboards only work if anyone can download the same instances. These pages publish the LP/MILP test sets used for official performance pages (Netlib and Kennington LPs, MILP collections) together with per-solver result tables.
## Core Contribution
- **Methodology:** Versioned instance archives (free/fixed MPS) plus published result tables per solver, updated over time under declared configurations.
- **Assumptions:** Instances are public and unmodified; results are computed with documented settings.
- **Benchmarks/datasets:** Netlib LP set, Kennington LP set, MILP benchmark sets.
- **Metrics:** Runtime per instance, geometric means, solved counts under limits.
- **Key results:** Provides both the data and the format our comparison tables should imitate (specific current numbers live on the site, not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** n/a — data source and reporting convention.
**Techniques:** Run published sets unmodified; compare with declared limits and settings; use geometric means across instances.
**Implementation details:** Our harness reads MPS via `src/io/mps.cpp` but has ingested only a handful of Netlib files; these archives are the queue for widening R15 coverage.
**Limitations/failure cases:** Sets include instances far beyond our current capability (large PILOT-class LPs) — expect timeouts that must be reported, not dropped.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R19 lists Mittelmann benchmark sets by name; this is where to obtain them and how the resulting numbers should be presented for R16.
## Evidence → Engineering Decision
- *Finding:* 12 Netlib LPs run (evidence/netlib_results.csv, netlib_extended.csv) vs. full published sets available → *PS requirement:* R19 → *Component:* src/io/mps.cpp → *Metric:* set coverage % and [[Geometric Mean Runtime]] over the full set
## Related Papers
- [[Mittelmann-n.d.-Benchmarks-Optimization-Software]]
- [[Anderssen-1984-NETLIB-LP-Test-Set]]
## Uses
- [[Mittelmann Benchmarks]]
