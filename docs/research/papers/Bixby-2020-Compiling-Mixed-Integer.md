---
type: paper
title: "Compiling Mixed Integer Programming Problems: The SCIP Optimization Suite"
authors: "Bixby, Hendel, Vigerske & Weninger"
year: 2020
venue: "MPC (Math. Prog. Comp.)"
doi: "(unverified)"
domain: [milp]
priority: ✦
status: standard
tags: [paper, milp]
---
# Compiling Mixed Integer Programming Problems: The SCIP Optimization Suite

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.

> Architecture of SCIP + SoPlex: how a from-scratch LP solver and an integer framework compose into one extensible system.
## Metadata
| Field | Value |
|---|---|
| Authors | Bixby, Hendel, Vigerske & Weninger |
| Year | 2020 |
| Venue | MPC (Mathematical Programming Computation) |
| DOI/URL | (unverified) — scipopt.org |
## Problem Addressed
SCIP grew organically and needed a paper explaining its layered design: SoPlex as LP solver, SCIP as MILP/MINLP framework, and "compiling" MPS into an internal representation with presolve — plus how the pieces stay decoupled.
## Core Contribution
- **Methodology:** System description: parsing/compilation to internal data structures, presolve as separate layer, LP interface abstraction (SoPlex), branch-cut-price loop, plugin architecture for handlers (cuts, heuristics, branching).
- **Assumptions:** C implementation with plugin registry; exact/rational LP option (SoPlex precision levels); single framework serving LP/MILP/MINLP.
- **Benchmarks/datasets:** MIPLIB 2017-era sets (suite's own experiments).
- **Metrics:** Solved instances; time; memory.
- **Key results:** SCIP as the leading open-source framework; suite composition gives LP + MILP + MINLP from shared pieces (qualitative).
## Engineering-Relevant Knowledge
**Algorithms:** Branch-cut-price loop; layered presolve; plugin dispatch.
**Techniques:** Internal model compilation (separate from MPS parsing), interface abstraction for LP engines.
**Implementation details:** Validates our separation: `src/io/mps.cpp` (parse) -> `src/transform/sparse_canonicalize.cpp` (canonical form) -> engines. Our LP engines (`src/lp/reference/`, `src/lp/dual/`, `src/lp/first_order/`) sit behind `--engine` selection, analogous to SoPlex swapping.
**Limitations/failure cases:** Plugin flexibility costs indirection overhead; documentation of tolerance interplay remains complex.
## Applicability to Our Project
**Classification:** Potentially applicable
**Why:** R1, R3 (modular architecture), R18 — reference for how to keep LP/MILP/QP engines modular and swappable in markov-cero without cross-coupling.
## Evidence → Engineering Decision
- *Finding:* Keeping engine interfaces uniform (`--engine auto`) matches proven suite design → *PS requirement:* R1, R3 → *Component:* apps/markov_cero_solve.cpp + src/milp/milp_solver.cpp (LP engine indirection) → *Metric:* same test suite passes across engines.
## Related Papers
- [[Achterberg-2005-General-Mixed-Integer]]
- [[Linderoth-2005-Noncommercial-Software-Mixed]]
## Uses
- [[Branch and Cut]]
