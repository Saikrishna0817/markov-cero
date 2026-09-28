---
type: paper
title: "Compact Mixed-Integer Programming Formulations in Quadratic Optimization"
authors: "Beach, Hildebrand & Huchette"
year: 2022
venue: "Journal of Global Optimization"
doi: "(unverified)"
domain: [miqp]
priority: ✦
status: standard
tags: [paper, miqp]
---
# Compact Mixed-Integer Programming Formulations in Quadratic Optimization
> Compact (non-extended) linearizations of quadratic optimization models — the formulation toolkit on an MIQP extension path would rest.
## Metadata
| Field | Value |
|---|---|
| Authors | Beach, Hildebrand & Huchette |
| Year | 2022 |
| Venue | Journal of Global Optimization |
| DOI/URL | https://link.springer.com/article/10.1007/s10898-022-01184-6 |

## Problem Addressed
Quadratic terms in mixed-integer models can be handled by (a) keeping them and solving MIQP subproblems, (b) linearizing into extended formulations with extra variables, or (c) compact reformulations. The paper studies compact formulations — few extra variables — and their strength relative to known extended alternatives.
## Core Contribution
- **Methodology:** Derive compact linear/linearized formulations for quadratic optimization models; compare relaxation strength and size against extended formulations; identify classes where compactness costs little in bound quality.
- **Assumptions:** Bounded variables (needed by convex/concave envelopes); integrality where present.
- **Benchmarks/datasets:** Quadratic assignment / binary-quadratic style test models from the literature (not re-verified).
- **Metrics:** LP-relaxation gap, formulation size (variables/constraints), solve time in an exact solver.
- **Key results:** Compact formulations can match extended formulations' relaxation quality on important classes with far fewer variables (figures not re-verified).
## Engineering-Relevant Knowledge
**Algorithms:** Linearization of quadratic objectives/constraints for branch-and-cut over LP relaxations.
**Techniques:** McCormick-style envelopes, bounded-variable reformulations, strength-vs-size trade-off analysis.
**Implementation details:** A future MIQP path (R3) can either (i) keep Q in node QPs — our `src/qp/` engines — or (ii) linearize via these formulations and reuse the MILP stack unchanged; (ii) is cheaper for us today.
**Limitations/failure cases:** Envelope quality depends on tight bounds; weak bounds (big-M style) reproduce the ill-conditioning R13 warns about.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R3 names MIQP as the first extension; choosing linearized-compact vs. QP-subproblem architecture early decides whether MIQP needs new node machinery at all.
## Evidence → Engineering Decision
- *Finding:* QP exists (`src/qp/admm_solver.cpp`) but no quadratic term enters the MILP path → *PS requirement:* R3 → *Component:* src/milp/milp_solver.cpp → *Metric:* MIQP instance solved via linearization vs. QP subproblem (roadmap)
## Related Papers
- [[Beach-2024-Enhancements-Discretization-Approaches]]
- [[Kronqvist-2025-50-Years-Mixed-Integer]]
## Uses
- [[Branch and Bound]]
