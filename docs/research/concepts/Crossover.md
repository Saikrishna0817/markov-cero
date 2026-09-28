---
type: concept
tags: [concepts, lp]
status: stable
verified_on: 2026-09-25
---

# Crossover

> Interior-point lands in the interior; crossover walks you to a vertex so simplex, warm starts and MIP can take over.

## Definition
Crossover is the procedure that converts an interior-point (barrier) solution — optimal but typically non-extreme, with all variables strictly positive — into an optimal basic solution. It identifies the candidate active set, drives free/nearly-zero variables to their bounds, adds fixing constraints, and runs usually the dual simplex on the resulting system until an optimal basis is reached. The output is a vertex plus a basis that every downstream consumer (MIP node LPs, sensitivity analysis, reduced-cost fixing, re-optimization after cuts) requires. Without crossover, a barrier solver produces objective values but no vertex, no basis and no warm start.

## Why It Matters Here
- R4 explicitly asks for "revised simplex **and** interior-point methods"; in the literature IPM and crossover come as a pair (Ye 1998), which is why the missing crossover is tracked alongside the missing IPM.
- Observed state: audit Phase 0 records "no crossover/basis extraction exists" (docs/audit/00-ground-truth.md, C.3) and PS-GAP-06 "No interior-point engine".
- Inference: because node LPs are warm-started from parent bases, the absence of crossover also caps how far a future IPM could be integrated into the MILP path.

## Key Facts / Rules
- Crossover LP = original model + constraints fixing the nonbasic-for-barrier variables; degeneracy is expected and handled by the dual simplex's tolerances.
- Ye (1998) frames crossover as an essential companion to path-following methods, not an optional extra.
- The basis produced must be certified (KKT residual) exactly like a simplex-produced one.
- A basis extracted at machine-precision barrier accuracy still needs verification after crossover pivots.

## Related
- [[Interior-Point Method]]
- [[Basis]]
- [[Warm Start]]
- [[No Crossover]]
- [[Ye-1998-Crossover-Interior-Point]]

## Referenced By

- [[15-roadmap|audit/15-roadmap]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Research MOC|research/Research MOC]]
- [[Interior-Point Method|research/algorithms/Interior-Point Method]]
- [[Warm Start|research/concepts/Warm Start]]
- [[No Crossover|research/limitations/No Crossover]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]
- [[Wright-1997-Primal-Dual-Interior-Point-Methods|research/papers/Wright-1997-Primal-Dual-Interior-Point-Methods]]
- [[Wright-2004-Interior-Point-Revolution|research/papers/Wright-2004-Interior-Point-Revolution]]
- [[Ye-1998-Crossover-Interior-Point|research/papers/Ye-1998-Crossover-Interior-Point]]
- [[No Interior-Point Engine|research/research-gaps/No Interior-Point Engine]]