---
type: paper
title: "Linear Programming Implementations of the Lift-and-Project Method"
authors: "Benichou, Gauthier, Girard & Sierksma"
year: 1997
venue: "IJOC"
doi: "(unverified)"
domain: [cuts]
priority: ○
status: standard
tags: [paper, cuts]
---

# Linear Programming Implementations of the Lift-and-Project Method

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Engineering report on running lift-and-project cuts inside a production LP solver (OSL): what actually works when the theory meets floating point.

## Metadata
| Field | Value |
|---|---|
| Authors | Benichou, Gauthier, Girard & Sierksma |
| Year | 1997 |
| Venue | INFORMS Journal on Computing (list: "IJOC") |
| DOI/URL | (unverified) |

## Problem Addressed
Balas et al. (#96) proved lift-and-project works in principle; implementing it requires choices the theory does not fix: which variables to split, how many cuts to add per round, how to keep the LP sparse and stable, when to stop.

## Core Contribution
- **Methodology:** Report on L&P embedded in the OSL solver: candidate screening, batch cut insertion, cut management, interaction with the LP factorization and with branch-and-bound.
- **Assumptions:** Industrial LP engine with warm-start/perturbation support; 0/1 models.
- **Benchmarks/datasets:** OSL-era MIP test problems (not itemized in list).
- **Metrics:** Nodes, time, cuts added per solve, LP overhead.
- **Key results:** Practical lessons — cut batching, candidate limits and stopping rules determine whether L&P pays for itself; it is expensive per cut but closes gaps that rounding cuts cannot.

## Engineering-Relevant Knowledge
**Algorithms:** L&P separation loop with LP re-optimization; candidate variable selection heuristics.
**Techniques:** Batched cut addition (reduce refactorizations), cut-count caps, early termination when no violation found.
**Implementation details:** Directly applicable to `src/milp/cut_pool.cpp` policy: cap, batch, and re-optimize by [[Dual Simplex]] rather than refactorizing from scratch. Our root-only policy means batching concerns are currently untested.
**Equations/rules:** Violation test: cut Σ a_j x_j ≤ b violated if Σ a_j x̄_j > b + ε with ε tied to LP feasibility tolerance (ties to [[Harris Ratio Test]]-style tolerances).
**Limitations/failure cases:** High LP overhead per cut; dense cuts; performance highly sensitive to candidate limits — an untuned implementation can be slower than no cuts.

## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** It is the implementation playbook for a cut system we already half-have (pool + root separation); the tuning rules transfer even if we never ship full L&P.

## Evidence → Engineering Decision
- *Finding:* cut management (cap/batch) decides whether cuts pay off → *PS requirement:* R5, R20 → *Component:* src/milp/cut_pool.cpp → *Metric:* LP time per node, [[Cut Efficiency]]

## Related Papers
- [[Balas-1993-Lift-Project-Cutting]]
- [[Padberg-2005-Classical-Cuts-Mixed]]
- [[Turner-2024-Potential-Cutting-Planes]]
- [[Balas-1996-Gomory-Cuts-Revisited]]

## Uses
- [[Cut Pooling]] [[Dual Simplex]] [[Cut Validity]] [[Harris Ratio Test]]
