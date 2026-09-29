---
type: architecture
tags: [architectures, milp]
status: stable
verified_on: 2026-09-25
---

# Presolve-Postsolve Stack

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Record every reduction as a reversible operation, then replay them backwards — the architecture that makes aggressive presolve safe.

## Definition
The presolve-postsolve stack is a log-structured pattern: each reduction (delete empty row, fix variable, substitute singleton) appends a record to an ordered stack containing enough data to invert the transformation, and postsolve walks that stack in reverse (LIFO) to rebuild the original-space primal solution and reconstruct dual values for eliminated entities. Because reductions are layered — a fixed variable can be the subject of a singleton substitution added earlier — the *order* of the log is part of the algorithm, not an implementation detail. A well-formed design guarantees: the reduced model's optimum maps back to a feasible original solution, duals reconstruct consistently, and the objective recomputed in original space matches the reduced solve.

## Why It Matters Here
- R5 requires presolve, and R17 requires *trusted* results — an unreversible presolve would make verification impossible.
- Observed state: `ReductionRecord = variant<EmptyRow, EmptyColumn, FixedVariable, RowSingleton>`; `postsolve` pops records LIFO, reconstructs a fixed-variable dual via π_i = (c_k − Σ a_rk π_r)/a_ik and recomputes the objective in original canonical space (src/presolve/presolve.cpp:255-308).
- Observed state: only four record types exist and there is no implied-bound/coefficient tightening (docs/codebase/components/Presolve.md) — Inference: the *architecture* supports more reductions than the *rule set* currently provides.

## Key Facts / Rules
- Every reduction needs an inverse map for primal values and, for eliminated rows/columns, a dual recovery rule.
- LIFO replay order must equal insertion order reversed; interleaving other transforms (scaling) requires a defined nesting.
- Termination statuses discovered during presolve (infeasible/unbounded) must also map back to original coordinates.
- Statistics per reduction class make presolve behavior auditable — a prerequisite for R17 evidence.

## Related
- [[Presolve]]
- [[CSC Sparse Model]]
- [[Multi-Engine Solver Architecture]]
- [[Scaling]]
- [[Andersen-1995-Presolving-Linear-Programming]]

## Referenced By

- 07-current-architecture
- 21-traceability
- Codebase MOC
- Architecture MOC
- Research MOC
- [[CSC Sparse Model|research/architectures/CSC Sparse Model]]
- [[Multi-Engine Solver Architecture|research/architectures/Multi-Engine Solver Architecture]]
- [[Presolve|research/concepts/Presolve]]
- [[ED-010-presolve-depth-over-new-engine|research/engineering-decisions/ED-010-presolve-depth-over-new-engine]]
- cross-paper-synthesis
- research-dependency-map