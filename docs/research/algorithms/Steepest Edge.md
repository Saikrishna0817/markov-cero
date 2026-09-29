---
type: concept
tags: [algorithms, lp]
status: stable
verified_on: 2026-09-25
---

# Steepest Edge

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> Rank candidate pivots by improvement *per unit of geometric step* — the pricing rule that survives degeneracy, and the one this solver does not have yet.

## Definition
Standard pricing picks the column with the most negative reduced cost, ignoring how long the step in that direction actually is. Steepest edge scores each candidate by dⱼ divided by the Euclidean length of the unit trajectory of the corresponding basic-variable movement (maintained as edge weights updated after each pivot), so it maximizes objective decrease per unit step in x-space. Exact weights are expensive to refresh, so practical codes use Devex-style approximate weights (Harris 1973) or periodic recomputation. On degenerate and poorly scaled LPs steepest edge avoids the long series of zero/tiny-step pivots that dₑᵥ pricing invites.

## Why It Matters Here
- R13 (degenerate/ill-conditioned instances) is precisely the setting where pricing rule choice decides iteration counts; literature priority reads (Goldfarb–Reid 1977; Goldfarb–Forrest 1992) flag it as essential for dual simplex.
- Observed state: **not implemented** — pricing is Bland/first-negative or most-negative in the reference engine, and `tableau_norm` in the dual engine carries an in-header warning that it "must not be advertised as exact DSE" (include/markov_cero/lp/dual/dual_simplex.hpp:12-14). README lists steepest-edge as planned Phase 8.
- Inference: this is the algorithmic half of the [[Bland-Only Pricing]] limitation and of part of the degeneracy-handling gap.

## Key Facts / Rules
- Score αⱼ = dⱼ / ‖direction_j‖₂ (unit-length normalization of the entering direction).
- Weight update after a pivot is O(m); recomputing all weights is O(m·nnz) — hence approximations.
- Devex = steepest edge with a cheaper approximate norm (approximate unit-length pricing).
- Steepest edge does not fix cycling by itself — a finite fallback (Bland) must remain.

## Related
- [[Reduced Cost]]
- [[Degeneracy]]
- [[Bland Anti-Cycling]]
- [[Dual Simplex]]
- [[Goldfarb-1992-Steepest-Edge-Simplex]]
- [[Fourer-1994-Steepest-Edge-Simplexing]]

## Referenced By

- 15-roadmap
- 21-traceability
- Algorithms MOC
- Architecture MOC
- Research MOC
- Research-Code Traceability MOC
- [[Dual Simplex|research/algorithms/Dual Simplex]]
- [[Degeneracy|research/concepts/Degeneracy]]
- [[Reduced Cost|research/concepts/Reduced Cost]]
- [[Bland-Only Pricing|research/limitations/Bland-Only Pricing]]
- cross-paper-synthesis
- [[Bixby-2002-Evolution-of-LP|research/papers/Bixby-2002-Evolution-of-LP]]
- [[Fourer-1994-Steepest-Edge-Simplexing|research/papers/Fourer-1994-Steepest-Edge-Simplexing]]
- [[Goldfarb-1977-Practicable-Steepest-Edge|research/papers/Goldfarb-1977-Practicable-Steepest-Edge]]
- [[Goldfarb-1992-Steepest-Edge-Simplex|research/papers/Goldfarb-1992-Steepest-Edge-Simplex]]
- [[Maros-0000-New-Degeneracy-Method|research/papers/Maros-0000-New-Degeneracy-Method]]
- [[Degeneracy Handling Gap|research/research-gaps/Degeneracy Handling Gap]]
- [[Bland Anti-Cycling|research/techniques/Bland Anti-Cycling]]