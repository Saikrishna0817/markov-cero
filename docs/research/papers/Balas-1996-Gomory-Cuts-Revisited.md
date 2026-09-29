---
type: paper
title: "Gomory Cuts Revisited"
authors: "Balas, Ceria, Cornuéjols & Natraj"
year: 1996
venue: "OR Letters"
doi: "(unverified)"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# Gomory Cuts Revisited

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Revived Gomory cuts from LP bases show that old cuts, correctly cut off the current relaxation, solve 86% of test problems vs. 55% without them.

## Metadata
| Field | Value |
|---|---|
| Authors | Balas, Ceria, Cornuéjols & Natraj |
| Year | 1996 |
| Venue | Operations Research Letters (list: "OR Letters") |
| DOI/URL | (unverified); list carries no link |

## Problem Addressed
Gomory cuts had been abandoned by the 1980s because implementations produced weak, redundant, numerically fragile inequalities and solvers got slower when they were used. The paper asks whether the failure was intrinsic or an artifact of implementation choices.

## Core Contribution
- **Methodology:** Generate mixed-integer Gomory cuts from the current LP basis, but (a) cut off only the current relaxation, (b) aggregate/select rows carefully, (c) re-optimize with a stable LP engine; compare with and without cuts.
- **Assumptions:** Cuts are separated at integer solutions of the relaxation (not at every node), LP solver tolerates added rows, floating-point with care.
- **Benchmarks/datasets:** MIPLIB-era test set (list reports "86% vs 55% solve rate on MIPLIB").
- **Metrics:** Percentage of instances solved to proven optimality; node counts; time.
- **Key results:** Reported ~86% solve rate with cuts vs. ~55% without — the number recorded in our reference list — showing the cut family was unduly written off.

## Engineering-Relevant Knowledge
**Algorithms:** Mixed Gomory cut generation from a basis, separation policy, cut deletion/rounding rules.
**Techniques:** Selective separation (only at integer incumbents / candidate solutions), coefficient tightening, duplicate elimination — the roots of [[Cut Pooling]].
**Implementation details:** Matches our setup (`src/milp/gomory.cpp` + `src/milp/cut_pool.cpp`); our current policy of applying cuts **only at the root** is weaker than this paper's "at integer solutions" policy, which is a plausible explanation for the measured 0.0% node reduction.
**Equations/rules:** Cut row selection: prefer rows with small denominators / favorable fractional parts; discard cuts that are parallel or dominated.
**Limitations/failure cases:** Requires a working LP with warm starts; adding many rows destroys sparsity and can slow nodes — needs a cut-selection cap (our `cut_pool.cpp` should bound active rows).

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** Same cut family, same era of LP engine; the reported solve-rate delta is the strongest literature-based argument for separating cuts inside the tree rather than only at root.

## Evidence → Engineering Decision
- *Finding:* 86% vs 55% solve rate with cuts → *PS requirement:* R5, R20 → *Component:* src/milp/cut_pool.cpp, src/milp/milp_solver.cpp → *Metric:* nodes solved, [[Relative Optimality Gap]]
- *Finding:* our cuts are root-only → *PS requirement:* R5 → *Component:* src/milp/milp_solver.cpp → *Metric:* node-count reduction ([[Cut Efficiency]])

## Related Papers
- [[Gomory-1963-All-Integer-Programming]]
- [[Padberg-2005-Classical-Cuts-Mixed]]
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Turner-2024-Potential-Cutting-Planes]]

## Uses
- [[Gomory Mixed Integer Cut]] [[Cut Pooling]] [[Branch and Cut]] [[Cut Validity]]
