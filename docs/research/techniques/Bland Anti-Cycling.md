---
type: concept
tags: [techniques, lp]
status: stable
verified_on: 2026-09-25
---

# Bland Anti-Cycling

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Always take the smallest admissible index — ugly, slow, and the only rule that provably terminates on degenerate LPs.

## Definition
Bland's rule resolves every tie deterministically: among columns with negative reduced cost choose the one with the smallest index, and break ratio-test ties with the smallest basis index (consistently). Because no basis can ever repeat under this discipline, the simplex must terminate in a finite number of steps on any LP, degenerate or not. The cost is pivot quality — the rule ignores reduced-cost magnitude and step geometry, so iteration counts on hard instances explode. The standard practice is therefore to run a good pricing rule normally and switch to Bland (or an EXPAND-style expanding tolerance) only when a stall or repeat is detected.

## Why It Matters Here
- R9 requires *reliable convergence*; a from-scratch solver without a finite-termination fallback can cycle forever on exactly the degenerate models R13 names.
- Observed state: `bland_anti_cycling` defaults to **true** in the reference engine, i.e. first-negative pricing with index tie-breaking (include/markov_cero/lp/reference/revised_simplex.hpp:23; src/lp/reference/revised_simplex.cpp:128-173) — Bland is the *primary* rule, not the fallback.
- Inference: this is safe for correctness but is the algorithmic core of the [[Bland-Only Pricing]] limitation: pivot quality is sacrificed by default.

## Key Facts / Rules
- Entering column = min{j : d_j < 0}; leaving row tie-break = smallest basis index ⇒ no basis repeats ⇒ finite termination.
- The guarantee is qualitative (termination), not a complexity bound — worst-case iteration counts are poor.
- Hybrid policies: normal pricing → detect stall (no objective improvement over k iterations) → Bland → resume.
- Alternatives with both guarantee and speed: EXPAND expanding tolerances, perturbation methods.

## Related
- [[Degeneracy]]
- [[Revised Simplex]]
- [[Steepest Edge]]
- [[Harris Ratio Test]]
- [[Bland-1977-Anti-Cycling-Rule]]
- [[Gill-1989-Practical-Anti-Cycling]]

## Referenced By

- Architecture MOC
- Research MOC
- [[Revised Simplex|research/algorithms/Revised Simplex]]
- [[Steepest Edge|research/algorithms/Steepest Edge]]
- [[Degeneracy|research/concepts/Degeneracy]]
- [[Bland-Only Pricing|research/limitations/Bland-Only Pricing]]
- cross-paper-synthesis
- [[Bland-1977-Anti-Cycling-Rule|research/papers/Bland-1977-Anti-Cycling-Rule]]
- [[Charnes-1954-Optimality-Multi-Valuedness|research/papers/Charnes-1954-Optimality-Multi-Valuedness]]
- [[Gill-1989-Practical-Anti-Cycling|research/papers/Gill-1989-Practical-Anti-Cycling]]
- [[Degeneracy Handling Gap|research/research-gaps/Degeneracy Handling Gap]]