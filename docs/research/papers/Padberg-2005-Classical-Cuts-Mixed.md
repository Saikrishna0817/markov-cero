---
type: paper
title: "Classical Cuts for Mixed-Integer Programming and Branch-and-Cut"
authors: "Padberg"
year: 2005
venue: "Ann. OR"
doi: "10.1007/s10479-005-3453-y"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# Classical Cuts for Mixed-Integer Programming and Branch-and-Cut

> Survey unifying fractional, cover, MIR and lift-and-project cuts and — critically for us — their validity once branching has changed the relaxation.

## Metadata
| Field | Value |
|---|---|
| Authors | Padberg |
| Year | 2005 |
| Venue | Annals of Operations Research 139 (list: "Ann. OR") |
| DOI/URL | 10.1007/s10479-005-3453-y (parsed from the RePEc link in the reference list) |

## Problem Addressed
Cut families are usually published one at a time with their own notation, so implementers cannot see how they relate or when a cut generated at the root remains valid deeper in the search tree.

## Core Contribution
- **Methodology:** Derive fractional/GMI, knapsack-cover, aggregation and lift-and-project cuts from one disjunctive framework; state conditions under which a cut stays valid after branching (tightening bounds change what must be checked).
- **Assumptions:** Bounded 0/1 or mixed-integer sets; cuts generated from valid row relaxations.
- **Benchmarks/datasets:** Survey — no new computational results are the point.
- **Metrics:** Cut strength (split rank), dominance relations between families.
- **Key results:** Catalogue that tells an implementer which family to reach for given row structure, and the validity-after-branching rules needed for a cut pool that survives the tree.

## Engineering-Relevant Knowledge
**Algorithms:** GMI/MIR, knapsack cover, aggregation, lift-and-project, split cuts — their derivation and containment relations.
**Techniques:** Re-validating pooled cuts after bound changes; discarding cuts that become redundant (cut-pool maintenance).
**Implementation details:** Our `src/milp/cut_pool.cpp` must decide whether a root-generated cut is re-checked at descendant nodes; Padberg supplies the validity conditions. Today cuts are root-only, so this rule is untested in our code.
**Equations/rules:** A cut Σ a_j x_j ≤ b derived with bounds l ≤ x ≤ u must be re-derived or re-verified when branching tightens l/u; bound changes can invalidate lifted coefficients (lift coefficients are computed for the original bounds).
**Limitations/failure cases:** Lifted coefficients are bound-dependent — forgetting to re-lift after branching silently produces *invalid* cuts, which shows up as infeasible "optimal" solutions.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** It is the manual for the cut pool we already have: which families to generate, how they relate, and the bound-dependence rule we must respect before applying cuts below the root.

## Evidence → Engineering Decision
- *Finding:* lifted cut coefficients depend on variable bounds → *PS requirement:* R9, R17 → *Component:* src/milp/cut_pool.cpp → *Metric:* feasibility of incumbents ([[KKT Residual]])
- *Finding:* root-only application leaves 0.0% node reduction → *PS requirement:* R5 → *Component:* src/milp/milp_solver.cpp → *Metric:* node-count reduction ([[Cut Efficiency]])

## Related Papers
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Balas-1980-Cuts-Fixed-Rank]]
- [[Gomory-1963-All-Integer-Programming]]
- [[Balas-1996-Gomory-Cuts-Revisited]]

## Uses
- [[Cut Validity]] [[Cut Pooling]] [[Branch and Cut]] [[Mixed Integer Rounding Cut]]
