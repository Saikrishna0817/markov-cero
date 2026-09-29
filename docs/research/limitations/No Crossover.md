---
type: limitation
tags: [limitations, lp]
status: addressed
verified_on: 2026-09-25
resolved_on: 2026-09-26
---

# No Crossover

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> There is no way to turn an interior solution into a basis — because there is no interior method to turn into one.

> **Resolved (2026-09-26).** Both halves now exist: `--engine ipm`
> (`src/lp/interior/ipm.cpp`) provides the interior method, and its crossover constructs a
> candidate basis from the interior iterate and certifies it through the dual simplex warm
> start. The returned `BasisState` is exportable (`--save-basis`) and reusable by node LPs,
> matching the [[Warm Start]] contract. Verified by `tests/ipm_test.cpp`
> (`test_crossover_basis_warm_start`); see also [[Crossover]].

## Definition
Crossover is the IPM→simplex handoff that drives a barrier solution to an optimal vertex and returns a basis. The limitation here is twofold: no interior-point engine exists to crossover *from*, and no crossover routine exists to accept a basis *into* the MIP pipeline. Consequences are structural rather than cosmetic — without a basis there is no warm start for node LPs from that solution, no reduced-cost fixing from it, no sensitivity analysis, and no way to demonstrate the IPM half of R4 even at small scale. A solver with first-order LP methods only can produce objective values, not vertices.

## Why It Matters Here
- R4 says "revised simplex **and** interior-point methods"; the missing crossover is the second half of the missing IPM and is tracked together with it (PS-GAP-06).
- Observed state: audit Phase 0 records "no crossover/basis extraction exists" (docs/audit/00-ground-truth.md C.3); `BasisState` warm starts originate only from simplex engines (src/lp/dual/dual_simplex.cpp:123-145).
- Inference: even if PDLP reached high accuracy, its output could not enter `solve_node_relaxation` today — the interface only accepts a basis.

## Key Facts / Rules
- Crossover LP = original model + fixing constraints for the interior solution's nonbasic set; solved by dual simplex.
- Degenerate crossover (many zeros at the boundary) is normal and is why Harris tolerances matter there too.
- Ye 1998 treats crossover as part of the IPM, not an add-on — a claim of "interior-point support" without it is incomplete.
- Alternative outputs (epigraph of active set, analytic-center crossover) exist but are all basis-producing.

## Related
- [[Crossover]]
- [[Interior-Point Method]]
- [[Warm Start]]
- [[No Interior-Point Engine]]
- [[Ye-1998-Crossover-Interior-Point]]

## Referenced By

- 21-traceability
- Architecture MOC
- Research MOC
- [[Crossover|research/concepts/Crossover]]
- [[ED-003-interior-point-required-by-ps|research/engineering-decisions/ED-003-interior-point-required-by-ps]]
- cross-paper-synthesis
- research-dependency-map
- [[No Interior-Point Engine|research/research-gaps/No Interior-Point Engine]]