---
type: problem-statement
id: SIH26119
title: Indigenous GPU-Accelerated Optimization Solver (Sovereign Alternative to Express / CPLEX)
organization: Mangalore Refinery and Petrochemicals Limited (MRPL)
department: Mangalore Refinery and Petrochemicals Limited (MRPL)
category: Software
theme: Smart Automation
idea_submission_deadline: 2026-09-30
registrations: 13/500
source: "https://sih.gov.in problem portal (verbatim, retrieved 2026-09-25)"
tags: [problem-statement, ground-truth, sih2026, lp, milp, qp, solver]
---

# SIH26119 — Verbatim Problem Statement

> **Ground truth status:** This file is the only verbatim copy of the problem statement inside
> the repository. It was recovered from the official SIH 2026 portal record on 2026-09-25.
> Prior to this file, the repo contained only paraphrases (see `docs/audit/00-ground-truth.md`,
> gap PS-GAP-01).

## Record metadata

| Field | Value |
|---|---|
| Problem Statement ID | 26119 |
| Title | Indigenous GPU-Accelerated Optimization Solver (Sovereign Alternative to Express / CEPLEX) |
| Organization | Mangalore Refinery and Petrochemicals Limited (MRPL) |
| Department | Mangalore Refinery and Petrochemicals Limited (MRPL) |
| Category | Software |
| Theme | Smart Automation |
| Idea submission deadline | 30 September 2026 |
| Registrations at retrieval | 13 / 500 |
| YouTube Link | (empty) |
| Dataset Link | (see "Dataset Link text" below — portal text truncated in retrieval) |

> **Note on title:** "CEPLEX" is the portal's OCR/rendering of **CPLEX**; the PS text
> alternates between "CPLEX" and the mangled "Express / CEPLEX" portmanteau of
> Xpress/CPLEX. Treat the intent as *sovereign alternative to Xpress/CPLEX*.

## Description (verbatim)

> **Background**
> Almost every optimization problem in India's refining, petrochemical, power, logistics, manufacturing and planning sectors ultimately depends on a handful of foreign mathematical optimization solvers such as IBM ILOG CPLEX, Gurobi and FICO Xpress. These engines sit behind refinery scheduling, production planning, supply chain optimization, blending, energy management and many AI-driven decision-support systems. While they are extremely capable, they come with high recurring license costs, restrictive licensing models and limited visibility into the underlying optimization algorithms. Indian developers can formulate optimization problems, but they cannot inspect, modify or tailor the solver internals to suit strategic national requirements. Open-source alternatives such as COIN-OR CBC, HiGHS, GLPK and SCIP exist and have made significant progress, but they still lag behind commercial solvers for several classes of large-scale mixed-integer optimization problems and have not been developed, validated or optimized specifically for Indian industrial use cases. The real challenge is not building the modeling interface; it is developing a numerically robust optimization engine that consistently finds high-quality solutions for large, sparse and highly constrained industrial problems within practical computation times.
>
> **Description**
> The objective is to develop a sovereign mathematical optimization solver core rather than a complete modeling environment. The solver should support Linear Programming (LP), Mixed-Integer Linear Programming (MILP) and Quadratic Programming (QP) as the initial focus, with a modular architecture that can later be extended to Mixed-Integer Quadratic Programming (MIQP), Nonlinear Programming (NLP) and Mixed-Integer Nonlinear Programming (MINLP). Core algorithms may include revised simplex and interior-point methods for continuous optimization, together with branch-and-bound, branch-and-cut, cutting planes, presolve, heuristics and advanced node selection strategies for mixed-integer problems. The solver should exploit sparse matrix techniques, efficient numerical linear algebra and multi-core parallelization, with GPU acceleration considered where it provides measurable benefits. The emphasis is on numerical stability, scalability and reliable convergence across large industrial optimization problems rather than on graphical interfaces or modelling tools. It shall not be built upon any existing open source solver library but shall be built from scratch from mathematical foundation. The scope is to solve optimization problems arising from refinery scheduling, crude blending, process optimization, production planning, logistics, power system dispatch, transportation and supply chain management. The benchmark is that the solver should consistently deliver optimal or near-optimal solutions for industrial-scale problems involving thousands to millions of variables and constraints, including highly degenerate models, ill-conditioned matrices and difficult mixed-integer formulations where weaker implementations exhibit excessive computation times or fail to converge.
>
> **Expected Solution**
> A robust optimization engine with a basic application programming interface (API) or command-line interface is sufficient; a polished graphical user interface is not required. The solver should successfully solve standard benchmark problems from recognised optimization libraries such as MIPLIB, Netlib or Mittelmann benchmark sets, with solution quality and computational performance compared against at least one established commercial or open-source solver. A clear demonstration of numerical robustness should be provided by solving challenging large-scale optimization problems involving degeneracy, weak LP relaxations or ill-conditioned constraint matrices, where simpler implementations struggle to achieve reliable convergence or acceptable solution times. The resulting solver should provide a transparent, extensible and sovereign foundation for future Indian optimization software across industrial, scientific and strategic applications.

## Dataset Link text (portal, partially truncated at retrieval)

> Teams to use publicly available mathematical optimization benchmark datasets such as MIPLIB, Netlib LP, Mittelmann benchmark instances, QPLIB (for quadratic programming where applicable), along with representative refinery scheduling, crude blending, production planning and supply chain optimization case studies from open literature. Where industrial da…

*(The portal field was cut off mid-sentence in the retrieved dump; the trailing clause likely qualifies the use of industrial data. Do not invent the missing text.)*

## Derived requirement checklist (audit input, each row traced later)

| # | Requirement (from PS text) | Clause quote anchor |
|---|---|---|
| R1 | Solver **core**, not modeling environment | "sovereign mathematical optimization solver core rather than a complete modeling environment" |
| R2 | LP, MILP, QP initial scope | "support LP, MILP and QP as the initial focus" |
| R3 | Extensible to MIQP, NLP, MINLP (modular architecture) | "modular architecture that can later be extended" |
| R4 | Revised simplex **and interior-point** methods | "revised simplex and interior-point methods" |
| R5 | Branch-and-bound, branch-and-cut, cutting planes, presolve, heuristics, advanced node selection | "together with branch-and-bound, branch-and-cut, ..." |
| R6 | Sparse matrix techniques + efficient numerical linear algebra | "exploit sparse matrix techniques" |
| R7 | Multi-core parallelization | "multi-core parallelization" |
| R8 | GPU acceleration **where measurable benefits** | "GPU acceleration considered where it provides measurable benefits" |
| R9 | Numerical stability, scalability, reliable convergence (emphasis) | "emphasis is on numerical stability..." |
| R10 | **No existing open-source solver library; from scratch from mathematical foundation** | "shall not be built upon any existing open source solver library" |
| R11 | Industrial scope: refinery scheduling, crude blending, production planning, logistics, power dispatch, transportation, supply chain | "The scope is to solve optimization problems arising from..." |
| R12 | Scale: thousands to millions of variables and constraints, sparse, highly constrained | "industrial-scale problems involving thousands to millions of variables and constraints" |
| R13 | Robustness: degeneracy, ill-conditioning, difficult MI relaxations | "including highly degenerate models, ill-conditioned matrices..." |
| R14 | API **or** CLI sufficient; **no GUI required** | "basic API or command-line interface is sufficient" |
| R15 | Solve MIPLIB / Netlib / Mittelmann benchmarks | "standard benchmark problems from recognised optimization libraries" |
| R16 | **Compare against at least one established commercial or open-source solver** | "compared against at least one established commercial or open-source solver" |
| R17 | Demonstrate numerical robustness on hard instances | "clear demonstration of numerical robustness should be provided" |
| R18 | Transparent, extensible, sovereign foundation | "transparent, extensible and sovereign foundation" |
| R19 | Datasets: MIPLIB, Netlib LP, Mittelmann, QPLIB + refinery/blending/planning case studies | Dataset Link field |
| R20 | Benchmark goal: optimal or near-optimal on industrial-scale, faster than weaker implementations | "consistently deliver optimal or near-optimal solutions..." |

## Explicitly NOT required by the problem statement

- No GUI (R14).
- No ML/AI model, no training pipeline, no frontend, no database, no web service.
- No NLP/MINLP/MIQP in the initial scope (R3 — *later extension* only).
- No deployment/infra requirement beyond being usable as API/CLI.
- No requirement to beat commercial solvers — only to *compare* against one (R16).
