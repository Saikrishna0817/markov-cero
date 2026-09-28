---
type: paper
title: "Machine Learning Techniques for Branch-and-Cut Methods: The Selection of Cutting Planes"
authors: "Giallombardo, Miglionico & Sammarra"
year: 2025
venue: "LNCS 14476, NUMTA 2023"
doi: "10.1007/978-3-031-81241-5_25"
domain: [cuts]
priority: ✦
status: standard
tags: [paper, cuts]
---

# Machine Learning Techniques for Branch-and-Cut Methods: The Selection of Cutting Planes

> SVR-based ranking of candidate cuts so the solver adds the fewest, most useful inequalities per node.

## Metadata
| Field | Value |
|---|---|
| Authors | Giallombardo, Miglionico & Sammarra |
| Year | 2025 (conference NUMTA 2023, published LNCS 2025) |
| Venue | LNCS 14476, NUMTA 2023 proceedings |
| DOI/URL | 10.1007/978-3-031-81241-5_25 |

## Problem Addressed
Adding every generated cut bloats the LP and slows nodes; heuristically keeping the "obvious" ones wastes better cuts. The paper asks whether a regressor can predict a cut's usefulness and rank candidates accordingly.

## Core Contribution
- **Methodology:** Extract features per candidate cut (violation, density, age, family, coefficient statistics), train support-vector regression to predict utility, and add only top-ranked cuts each round.
- **Assumptions:** Feature set computable cheaply at separation time; training instances drawn from the same distribution as test instances.
- **Benchmarks/datasets:** MIP instances from the NUMTA study (not itemized in list).
- **Metrics:** Solve time/nodes vs. naive "add all" and vs. simple deterministic filters.
- **Key results:** Learned selection can beat indiscriminate cut addition — the ML analogue of #104's "potential" argument.

## Engineering-Relevant Knowledge
**Algorithms:** Feature-based cut scoring; top-k selection with per-node budget.
**Techniques:** SVR regression on cut features; offline training + online inference (must be deterministic and fast in a from-scratch solver).
**Implementation details:** A deterministic score in `src/milp/cut_pool.cpp` (violation/age/density) is the zero-ML baseline that must be beaten first; PS does not require ML (explicitly not required).
**Equations/rules:** score(cut) = w·features (SVR kernel in paper); keep cut if score > threshold and count < budget.
**Limitations/failure cases:** Requires training data and a feature pipeline; inference cost and reproducibility risk; explicitly out of PS scope (no ML required).

## Applicability to Our Project
**Classification:** Not applicable
**Why:** R16/R5 need better cut policy, but the PS states no ML requirement and our first step must be a deterministic selection rule; note kept for the cut-selection framing and future optionality.

## Evidence → Engineering Decision
- *Finding:* learned selection only helps after a deterministic baseline → *PS requirement:* R5 → *Component:* src/milp/cut_pool.cpp → *Metric:* [[Cut Efficiency]], [[Geometric Mean Runtime]]

## Related Papers
- [[Turner-2024-Potential-Cutting-Planes]]
- [[Benichou-1997-Linear-Programming-Implementations]]
- [[Padberg-2005-Classical-Cuts-Mixed]]
- [[Chung-2015-Computational-Study-Cutting]]

## Uses
- [[Cut Pooling]] [[Cut Efficiency]] [[Branch and Cut]] [[Weak Relaxation]]
