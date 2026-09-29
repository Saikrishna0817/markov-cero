---
type: paper
title: "Production Planning by Mixed Integer Programming (supply-chain arc-flow/lot-sizing literature)"
authors: "Pochet & Wolsey; Chvátal (as listed)"
year: 2006
venue: "Springer (book)"
doi: "(unverified)"
domain: [domain]
priority: ○
status: standard
tags: [paper, domain]
---
# Production Planning by Mixed Integer Programming (supply-chain arc-flow/lot-sizing literature)

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> The canonical supply-chain/lot-sizing MIP formulations (arc-flow, multi-item capacitated lot-sizing) behind R11's supply-chain scope.
## Metadata
| Field | Value |
|---|---|
| Authors | Pochet & Wolsey (2006); also cites Chvátal arc-flow literature |
| Year | 2006 (no ★/✦/○ marker on this entry in source list; classified ○) |
| Venue | Springer (book) |
| DOI/URL | (unverified) — Errata note: domain-application entries still need DOI lookups |

## Problem Addressed
Supply chains couple lot-sizing decisions (fixed-charge production batches) with network flow (arc capacities, inventory, multi-echelon distribution). The work systematizes how these problems are modeled as MIPs and which formulations give relaxations strong enough to solve.
## Core Contribution
- **Methodology:** MIP formulations for lot-sizing and supply-chain networks: single/multi-item capacitated models, arc-flow and commodity-flow representations, valid inequalities and tightening results.
- **Assumptions:** Deterministic demands; linear costs with fixed charges; discrete time buckets.
- **Benchmarks/datasets:** Literature planning instances (not re-verified).
- **Metrics:** Total cost objective, gap closed by strengthened formulations, solve time.
- **Key results:** Strengthened formulations and valid inequalities reduce branch-and-bound trees by orders of magnitude (qualitative restatement of the literature's main lesson).
## Engineering-Relevant Knowledge
**Algorithms:** Lot-sizing MILP; multi-commodity network formulations.
**Techniques:** Tightening via valid inequalities, aggregation/disaggregation, big-M avoidance where possible.
**Implementation details:** These are exactly the "large, sparse, highly constrained industrial problems" R12 describes; they stress presolve (`src/presolve/presolve.cpp`) and cuts, both still immature (root cut node reduction 0.0% in phase4.json).
**Limitations/failure cases:** Big-M formulations trigger [[Ill-Conditioning]] and weak relaxations — the failure mode R13 warns about.
## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Supply chain management is named verbatim in R11; this is the formulation reference for building credible non-toy case studies for R19.
## Evidence → Engineering Decision
- *Finding:* Root cuts show 0.0% node reduction (evidence/benchmarks/phase4.json) while industrial formulations rely on cuts/branching → *PS requirement:* R5 → *Component:* src/milp/cut_pool.cpp → *Metric:* [[Cut Efficiency]] (node reduction at root)
## Related Papers
- [[Kallrath-2002-Planning-Scheduling-Industry]]
- [[Toth-2014-Vehicle-Routing-Problems]]
## Uses
- [[Branch and Bound]]
