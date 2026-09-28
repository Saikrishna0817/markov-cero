---
type: architecture
tags: [architectures, solver]
status: stable
verified_on: 2026-09-25
---

# Multi-Engine Solver Architecture

> One canonical model, several engines behind it — the design that lets simplex, first-order and ADMM coexist without duplicating the pipeline.

## Definition
A multi-engine solver architecture separates the front end (parsing, canonicalization, presolve, scaling) from interchangeable optimization back ends (revised simplex, dual simplex, PDHG/PDLP on CPU or GPU, ADMM for QP) behind a common model representation and a common result/certification contract. Dispatch is decided per problem class and per option — LP vs MILP vs QP, cold vs warm start, CPU vs GPU backend — while verification, postsolve and output serialization stay engine-independent. The payoff is extensibility (new engines slot in without touching I/O) and honest comparison (engines can be A/B tested on identical inputs); the cost is a shared interface that must be rich enough for basis warm starts, certificates and residuals.

## Why It Matters Here
- R1 (solver core, not modeling environment), R3 (modular architecture extensible to MIQP/NLP/MINLP) and R4 (multiple LP methods) are all architectural requirements — this is the shape that satisfies them.
- Observed state: dispatch exists today — LP: revised/dual/pdlp (CPU→GPU), MILP: branch-and-cut (+ `--engine parallel`), QP/MIQP: ADMM with convexity screening (apps/markov_cero_solve.cpp:239-240; src/milp/node_lp.cpp:15-77).
- Observed state: **no interior-point engine is behind the interface** (PS-GAP-06) — Inference: R4's "and interior-point methods" requirement is the missing slot, with crossover as its required companion.

## Key Facts / Rules
- Shared stages: MPS parse → canonicalize (CSC) → presolve → scale → engine → verify → postsolve → JSON.
- Contract per engine: status + objective + primal/dual vectors + residuals/certificates, so verification is uniform.
- Warm start (`BasisState`) only makes sense for basis-producing engines — the interface must express its absence.
- Engine choice policy should be data-driven (size, density, integrality), not hard-coded per problem class.

## Related
- [[CSC Sparse Model]]
- [[Presolve-Postsolve Stack]]
- [[Interior-Point Method]]
- [[RevisedSimplexEngine]]
- [[PDLP-Engine]]
- [[QP-ADMM-Engine]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Codebase MOC|codebase/Codebase MOC]]
- [[Research MOC|research/Research MOC]]
- [[CSC Sparse Model|research/architectures/CSC Sparse Model]]
- [[Presolve-Postsolve Stack|research/architectures/Presolve-Postsolve Stack]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Lin-2025-PDCS-Primal-Dual|research/papers/Lin-2025-PDCS-Primal-Dual]]