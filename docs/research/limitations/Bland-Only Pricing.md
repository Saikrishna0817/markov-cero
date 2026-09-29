---
type: limitation
tags: [limitations, lp]
status: stable
verified_on: 2026-09-25
---

# Bland-Only Pricing

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The primary pricing rule is "first admissible index" — guaranteed finite, and blind to which pivot is actually good.

## Definition
Pricing determines which column enters the basis; the current rule is Bland-style (smallest index with negative reduced cost) in the reference engine, and a `tableau_norm` policy in the dual engine that its own header says must not be described as dual steepest-edge. Bland's rule guarantees termination but ignores reduced-cost magnitude and step geometry, so it takes many small pivots where steepest edge would take few large ones — on degenerate or ill-conditioned instances the iteration-count penalty is severe. The limitation is that the *quality* axis of pricing is entirely unimplemented, leaving anti-cycling as the only pricing behavior available.

## Why It Matters Here
- R13/R9: the hard instances SIH asks about are exactly where pricing quality decides convergence; R20 pays for every extra iteration at every node.
- Observed state: `bland_anti_cycling` default true (include/markov_cero/lp/reference/revised_simplex.hpp:23; src/lp/reference/revised_simplex.cpp:128-151); dual pricing documented as not-DSE (include/markov_cero/lp/dual/dual_simplex.hpp:12-14); README lists steepest-edge as planned Phase 8.
- Inference: measured iteration counts cannot currently be attributed to algorithmic quality vs the safety rule, because there is no alternative pricing to compare against.

## Key Facts / Rules
- Bland guarantees finite termination; it provides no iteration-quality guarantee (worst case is poor).
- Devex/steepest edge rank candidates by improvement per unit step — the standard fix, cheap in approximate form.
- Correct architecture: good pricing as default + Bland as stall-detected fallback (never replace the guarantee, only demote it).
- Dual simplex needs its own steepest-edge variant (dual DSE), which is a separate implementation from primal.

## Related
- [[Bland Anti-Cycling]]
- [[Steepest Edge]]
- [[Degeneracy]]
- [[Reduced Cost]]
- [[Bland-1977-Anti-Cycling-Rule]]

## Referenced By

- 21-traceability
- Algorithms MOC
- Research-Code Traceability MOC
- [[Steepest Edge|research/algorithms/Steepest Edge]]
- [[ED-002-keep-simplex-core-add-first-order-not-replace|research/engineering-decisions/ED-002-keep-simplex-core-add-first-order-not-replace]]
- cross-paper-synthesis
- research-dependency-map
- [[Bland Anti-Cycling|research/techniques/Bland Anti-Cycling]]