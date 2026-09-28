---
type: paper
title: "50 Years of Mixed-Integer Nonlinear and Disjunctive Programming"
authors: "Kronqvist, Bernal & Grossmann"
year: 2025
venue: "European Journal of Operational Research 331(3):687–705"
doi: "10.1016/j.ejor.2025.07.016"
domain: [minlp]
priority: ✦
status: standard
tags: [paper, minlp]
---
# 50 Years of Mixed-Integer Nonlinear and Disjunctive Programming
> Half-century survey of MINLP and generalized disjunctive programming — the map of methods a sovereign solver would inherit when it crosses from MILP into nonlinear models.
## Metadata
| Field | Value |
|---|---|
| Authors | Kronqvist, Bernal & Grossmann |
| Year | 2025 |
| Venue | European Journal of Operational Research 331(3):687–705 |
| DOI/URL | 10.1016/j.ejor.2025.07.016 |

## Problem Addressed
MINLP combines combinatorial choice with nonlinear physics, and the literature spans decades of divergent traditions (GDP, OA, Benders, spatial B&B). The survey organizes these methods, their assumptions and their solver implementations so newcomers can navigate the field.
## Core Contribution
- **Methodology:** Survey of MINLP algorithms — outer approximation, extended cutting plane, generalized Benders, spatial branch-and-bound, GDP reformulations — plus the disjunctive programming lineage and modern solver ecosystems.
- **Assumptions:** Reader knows LP/MIP and basic nonlinear optimization; covers convex and nonconvex regimes.
- **Benchmarks/datasets:** Classic MINLP collections cited in the survey (not re-verified).
- **Metrics:** Method taxonomy by problem class (convex MINLP, nonconvex, GDP) rather than empirical tables.
- **Key results:** Convex MINLP is largely "solved" by OA/Benders-style methods on top of MILP+NLP cores; nonconvex remains dominated by spatial branch-and-bound (qualitative restatement).
## Engineering-Relevant Knowledge
**Algorithms:** Outer approximation, extended cutting plane, generalized Benders, spatial branch-and-bound, GDP.
**Techniques:** Iterative linearization around NLP solutions; MILP master + NLP subproblem decomposition — reuses an LP/MILP core exactly like ours.
**Implementation details:** This architecture (MILP core + NLP subproblem layer) is the natural R3 extension path: our `src/milp/` master plus a future NLP oracle, rather than a rewrite.
**Limitations/failure cases:** Requires a reliable NLP solver (none in-repo); GDP reformulations inflate model size.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R3 lists MINLP as a later extension; the survey tells us the extension is incremental (add NLP subproblems + OA cuts) if the MILP core is clean.
## Evidence → Engineering Decision
- *Finding:* No NLP components exist; MINLP is explicitly out of initial scope (R3, PS "later extension") → *PS requirement:* R3 → *Component:* src/milp/milp_solver.cpp → *Metric:* extension readiness (interface for NLP subproblem oracle)
## Related Papers
- [[Linan-2025-Trends-Perspectives-Deterministic]]
- [[Beach-2022-Compact-Mixed-Integer]]
## Uses
- [[Interior-Point Method]]
