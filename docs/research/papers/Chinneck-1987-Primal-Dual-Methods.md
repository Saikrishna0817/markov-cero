---
type: paper
title: "Primal-Dual Methods for LP with Ill-Conditioning"
authors: "Chinneck & Drud"
year: 1987
venue: "(not stated in list)"
doi: "(unverified)"
domain: [numerics]
priority: ○
status: standard
tags: [paper, numerics]
---

# Primal-Dual Methods for LP with Ill-Conditioning

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Handling near-singular LP systems: detect ill-conditioning, diagnose whether it is inherent or artificial, and recover rather than crash.

## Metadata
| Field | Value |
|---|---|
| Authors | Chinneck & Drud |
| Year | 1987 |
| Venue | (not stated in reference list) |
| DOI/URL | (unverified) |

## Problem Addressed
When an LP's constraints are nearly dependent (rank-deficient or tightly coupled rows), factorizations break down or produce wildly wrong pivots. Solvers typically abort; the paper treats ill-conditioning as a diagnosable, partly repairable condition.

## Core Contribution
- **Methodology:** Combine primal and dual information to identify ill-conditioned rows/columns, classify the source (inherent problem conditioning vs. scaling vs. redundant rows), and apply corrective steps (scaling, row removal, perturbation) before continuing.
- **Assumptions:** Access to primal and dual iterates; ability to modify or rescale the model.
- **Benchmarks/datasets:** Ill-conditioned industrial LPs.
- **Metrics:** Condition improvement after repair; solve success rate on hard instances.
- **Key results:** Ill-conditioning is often *treatable* (scaling, dependent-row removal) rather than fatal — but only if detected early.

## Engineering-Relevant Knowledge
**Algorithms:** Ill-conditioning diagnosis and repair in primal-dual simplex settings.
**Techniques:** Row/column scaling (we have Ruiz: `src/scale/ruiz_scaling.cpp`), dependent-row detection (our presolve lacks it — cf. O'Leary 1981, out of scope), continued solve after repair.
**Implementation details:** Ties together what we have (Ruiz scaling, 4 presolve rules) with what is missing (diagnosis). A precondition for a credible R17 "challenging ill-conditioned" demonstration.
**Equations/rules:** none needed beyond κ̂ monitoring (#144) triggering repair actions.
**Limitations/failure cases:** Some instances are intrinsically ill-conditioned (no repair works — then certify with #153/#155 instead).

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** Provides the "recover, don't crash" policy for R13/R17; most of its first steps (scaling, detection) we can assemble from existing parts.

## Evidence → Engineering Decision
- *Finding:* `BLEND → NumericalFailure` with no diagnosis → *PS requirement:* R13, R17 → *Component:* src/scale/ruiz_scaling.cpp, src/presolve/presolve.cpp, src/lp/dual/dual_simplex.cpp → *Metric:* hard-instance solve rate, [[Ill-Conditioning]]

## Related Papers
- [[Renegar-1994-Condition-Numbers-Linear]]
- [[Cline-1979-Estimate-Condition-Number]]
- [[Charnes-1954-Optimality-Multi-Valuedness]]
- [[Neumaier-2004-Safe-Bounds-Linear]]

## Uses
- [[Ill-Conditioning]] [[Scaling]] [[Degeneracy]] [[Dual Simplex]]
