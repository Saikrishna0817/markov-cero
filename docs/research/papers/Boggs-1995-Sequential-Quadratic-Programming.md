---
type: paper
title: "Sequential Quadratic Programming"
authors: "Boggs & Tolle"
year: 1995
venue: "Acta Numerica"
doi: "(unverified)"
domain: [qp]
priority: ○
status: standard
tags: [paper, qp]
---
# Sequential Quadratic Programming

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Acta Numerica survey of SQP — QP subproblems as the building block of the future NLP/MINLP extension.
## Metadata
| Field | Value |
|---|---|
| Authors | Boggs & Tolle |
| Year | 1995 |
| Venue | Acta Numerica |
| DOI/URL | (unverified) |
## Problem Addressed
NLP methods repeatedly solve QP subproblems; the field needed a rigorous account of SQP convergence, merit functions and QP-subproblem requirements. The paper consolidates the theory and practice of the era.
## Core Contribution
- **Methodology:** SQP: model Lagrangian as QP around current point, add merit/line-search globalization; convergence theory under constraint qualifications.
- **Assumptions:** Smooth constraints; QP subproblem solver (convex or indefinite QP); regularity (LICQ-ish) at solutions.
- **Benchmarks/datasets:** Classical NLP test problems (qualitative).
- **Metrics:** Convergence rate, QP subproblem count.
- **Key results:** Superlinear convergence under standard conditions when QP subproblems are solved accurately (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** SQP; QP subproblem solves.
**Techniques:** Merit functions; penalty updates; active-set/IPM QP solves as inner loop.
**Implementation details:** Irrelevant to today's LP/MILP/QP/MIQP scope; relevant only because R3 names NLP/MINLP as future extensions — our QP engines would become the inner solvers.
**Limitations/failure cases:** QP accuracy drives SQP accuracy; infeasible QP subproblems need elastic modes.
## Applicability to Our Project
**Classification:** Background knowledge
**Why:** R3 explicitly defers NLP/MINLP; this defines what the QP layer must eventually provide (accurate convex QP solves in iteration loops).
## Evidence → Engineering Decision
- *Finding:* Future NLP support depends on a reusable, accurate QP core → *PS requirement:* R2, R3 → *Component:* src/qp/admm_solver.cpp (keep QP interface solver-agnostic) → *Metric:* QP [[KKT Residual]] contract per solve.
## Related Papers
- [[Wright-1997-Primal-Dual-IPM]]
- [[Lawson-1974-Solving-Least-Squares]]
## Uses
- [[KKT Conditions]]
- [[Duality Gap]]
