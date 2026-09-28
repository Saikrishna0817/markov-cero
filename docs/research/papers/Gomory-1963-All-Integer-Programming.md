---
type: paper
title: "All-Integer Programming Algorithm / mixed-integer version"
authors: "Gomory"
year: 1963
venue: "Operations Research"
doi: "(unverified)"
domain: [cuts]
priority: ★
status: deep
tags: [paper, cuts]
---

# All-Integer Programming Algorithm / mixed-integer version

> The GMI cut: a mixed-integer rounding inequality derived from one LP row, still the workhorse cut family in modern solvers.

## Metadata
| Field | Value |
|---|---|
| Authors | Gomory |
| Year | 1963 |
| Venue | Operations Research (list: "Operations Research") |
| DOI/URL | (unverified) |

## Problem Addressed
The 1958 fractional cut requires every variable to be integer, which excludes almost all industrial models (continuous flows, quantities). A row containing only one integer variable must still be exploitable.

## Core Contribution
- **Methodology:** Derive an inequality from a single row with one integer variable by rounding on the fractional part of the right-hand side and rescaling coefficients above/below it; the cut is valid for every integer assignment of the single integer variable.
- **Assumptions:** Row contains exactly one integer variable (or an aggregation with one integer member), variables nonnegative after standardization, LP relaxation bounded at the vertex.
- **Benchmarks/datasets:** Hand examples; no suite in the original.
- **Metrics:** Gap closed per cut; number of cuts needed.
- **Key results:** GMI is the direct ancestor of the MIR family (Marchand–Wolsey #99) and is present in essentially every commercial/open MIP solver.

## Engineering-Relevant Knowledge
**Algorithms:** [[Gomory Mixed Integer Cut]] generation from a tableau/basis row; mixed-integer rounding.
**Techniques:** Split the row on f = frac(b); keep coefficients a_j ≤ f, rescale those above f; optionally take the mirrored form when f > 0.5.
**Implementation details:** Our implementation is `src/milp/gomory.cpp` (GMI) with `src/milp/mir.cpp` (MIR family); both are applied **only at the root node** today (audit C.3), which is why measured node reduction is 0.0%.
**Equations/rules:** For x_0 integer ≥ 0 with x_0 + Σ a_j x_j = b, f = b − ⌊b⌋ ∈ (0,1):
  Σ_{a_j ≤ f} a_j x_j + Σ_{a_j > f} ((1 − a_j + f)/(1 − f)) x_j ≥ f.
  *(Reproduce-check this against `src/milp/gomory.cpp` before relying on it — the sign/side convention changes with row orientation.)*
**Limitations/failure cases:** A single GMI cut per row is weak on knapsack/covering rows — cover (#100/#101) and aggregation (#102) cuts are the complement; repeated application can cycle without cut management.

## Applicability to Our Project
**Classification:** Directly applicable
**Why:** This is the algorithm already implemented; the note pins its validity conditions, the f > 0.5 mirror rule, and the fact that root-only application is the current bottleneck.

## Evidence → Engineering Decision
- *Finding:* GMI/MIR implemented but applied only at root, 0.0% node reduction → *PS requirement:* R5 → *Component:* src/milp/gomory.cpp, src/milp/milp_solver.cpp → *Metric:* node reduction %, [[Cut Efficiency]]
- *Finding:* cut formulas must match the written derivation → *PS requirement:* R9, R17 → *Component:* src/verify/primal_verifier.cpp → *Metric:* [[KKT Residual]], feasibility of returned incumbents

## Related Papers
- [[Gomory-1958-Outline-Algorithm-Integer]]
- [[Marchand-1996-Mixed-Integer-Rounding]]
- [[Balas-1996-Gomory-Cuts-Revisited]]
- [[Richard-2010-Group-Approach-Cutting]]

## Uses
- [[Gomory Mixed Integer Cut]] [[Mixed Integer Rounding Cut]] [[Cut Validity]] [[Dual Simplex]]
