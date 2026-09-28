---
type: concept
tags: [concepts, milp]
status: stable
verified_on: 2026-09-25
---

# Presolve

> Cheap structural reductions before the real solve — historically the single largest speedup a solver can buy.

## Definition
Presolve applies fast, provably equivalent transformations to a model before optimization: delete empty rows/columns, substitute row singletons, fix variables whose bounds force them, remove dominated and forcing constraints, detect implied bounds, aggregate redundant rows and split dense columns. Every reduction must be *reversible*: the solver records enough information (a reduction stack) to reconstruct the original-space primal solution and dual values after the reduced model is solved. Postsolve walks that stack in reverse and recomputes the objective in the original space so that verification is done against the model the user submitted.

## Why It Matters Here
- R5 names presolve explicitly; Bixby 2002 attributes order-of-magnitude historical gains to it, and it is the cheapest path toward R20's industrial-scale times.
- Observed state: only four rules are implemented — empty row, empty column, row singleton, fixed variable — with `max_passes 5` and LIFO `postsolve` that recovers a dual via πᵢ = (c_k − Σ a_rk π_r)/a_ik (src/presolve/presolve.cpp:67-177, 255-308).
- Inference (fact of absence): no implied-bound propagation, no coefficient-based tightening and no probing are present (noted as an open question in docs/codebase/components/Presolve.md), so the gap to Andersen & Andersen 1995 / Achterberg et al. 2020 is large.

## Key Facts / Rules
- Singleton row ⇒ substitute one variable out entirely; fixed column ⇒ remove and adjust RHS/objective.
- Dual recovery must invert the substitution chain exactly — hence a stack with a defined pop order.
- Presolve can prove infeasible/unbounded early, before any engine runs (early exit in apps/markov_cero_solve.cpp:283-287).
- Every reduction must preserve the *optimal set in original coordinates*, not just the objective value.

## Related
- [[Presolve-Postsolve Stack]]
- [[Scaling]]
- [[Weak Relaxation]]
- [[CSC Sparse Model]]
- [[Andersen-1995-Presolving-Linear-Programming]]
- [[Achterberg-2020-Presolve-Reductions-Mixed]]

## Referenced By

_Populated later by `scripts/link_backlinks.py`._
