---
type: paper
title: "Carpentier (1962) origin of economic dispatch; Cohen & Wan (1983)"
authors: "Carpentier; Cohen & Wan"
year: 1962
venue: "(unverified)"
doi: "(unverified)"
domain: [domain]
priority: ○
status: standard
tags: [paper, domain]
---
# Carpentier (1962) origin of economic dispatch; Cohen & Wan (1983)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Where power-system economic dispatch and unit commitment enter mathematical programming — the QP/LP and MILP roots of the PS's power-dispatch scope.
## Metadata
| Field | Value |
|---|---|
| Authors | Carpentier (1962); Cohen & Wan (1983) |
| Year | 1962 (first entry; no ★/✦/○ marker in source list; classified ○) |
| Venue | (unverified) |
| DOI/URL | (unverified) — Errata note: domain-application entries still need DOI lookups |

## Problem Addressed
A power system must allocate generation across units to meet demand at minimum cost (economic dispatch) while deciding which units are on at all (unit commitment). The entries mark the origin of casting both as mathematical programs — convex optimization for dispatch, MILP for commitment.
## Core Contribution
- **Methodology:** Economic dispatch as convex cost minimization with demand and generator-limit constraints (LP/QP form); unit commitment as MILP with binary on/off variables, start-up costs and min up/down times.
- **Assumptions:** Convex generation cost curves for dispatch; deterministic demand; commitment simplified (no network/security constraints in the basic forms).
- **Benchmarks/datasets:** Power-system test cases of the literature (not re-verified).
- **Metrics:** Production cost, constraint violation, solve time vs. required dispatch horizon.
- **Key results:** Dispatch is well suited to [[Interior-Point Method]]/QP engines; commitment needs branch-and-bound scale — the pairing of R2's LP/QP and MILP scopes (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Economic dispatch (QP/LP), unit commitment (MILP).
**Techniques:** Convex cost minimization for dispatch; big-M/min-up-time constraints for commitment; tight formulations reduce branch-and-bound size.
**Implementation details:** Our QP engine (`src/qp/admm_solver.cpp`, KKT via `src/qp/kkt.cpp`) and MILP path cover both halves of this use case; dispatch instances are a natural QP benchmark for R2/R19.
**Limitations/failure cases:** Unit commitment with network constraints explodes combinatorially — far beyond our current cut/branching maturity; start with single-bus dispatch and relaxed commitment.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** R11 lists power system dispatch explicitly; the pairing validates why the PS demands both QP and MILP rather than MILP alone.
## Evidence → Engineering Decision
- *Finding:* QP support exists (`src/qp/`) but no power-dispatch instance is present in benchmarks → *PS requirement:* R11 → *Component:* src/qp/admm_solver.cpp → *Metric:* optimality gap on a dispatch QP case study
## Related Papers
- [[Kallrath-2002-Planning-Scheduling-Industry]]
- [[Neiro-2004-Mathematical-Modeling-Petroleum]]
## Uses
- [[Branch and Bound]]
