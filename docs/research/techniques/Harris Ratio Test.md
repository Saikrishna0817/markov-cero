---
type: concept
tags: [techniques, lp]
status: stable
verified_on: 2026-09-25
---

# Harris Ratio Test

> Accept any row within a tolerance band of the minimum ratio, then take the biggest pivot among them — the ratio test every production simplex runs.

## Definition
The exact-arithmetic ratio test demands the strict minimum, which in floating point picks essentially arbitrary pivots among near-ties and destroys accuracy and sparsity. Harris's rule (1973) works in two passes: first find the minimum ratio under a tight (near-exact) tolerance, then re-scan accepting every row whose ratio lies within a multiplicative band (1+τ) of that minimum, and finally choose the *largest-magnitude* pivot among the accepted rows. Larger pivots round better and preserve sparsity; the band converts a knife-edge tie into a deliberate choice. The band width τ must be scale-relative and documented — too large admits real infeasibility, too small degenerates back to the exact rule.

## Why It Matters Here
- R9/R13 meet in the ratio test — the most safety-critical routine in simplex, and the one degenerate instances stress.
- Observed state: `harris_ratio` defaults to true in the dual engine (include/markov_cero/lp/dual/dual_simplex.hpp:32); the reference engine instead breaks ratio-test ties on the smallest basis index (src/lp/reference/revised_simplex.cpp:154-173).
- Inference: tolerance-based acceptance only makes sense on comparably scaled rows, which links this technique directly to [[Scaling]] — a gap on the unscaled MILP/QP branches.

## Key Facts / Rules
- Two-pass rule: pass 1 ⇒ min ratio with tight tolerance; pass 2 ⇒ accept ratioᵢ ≤ (1+τ)·min, select max |ȳᵢ|.
- τ is a multiplicative band, not an absolute epsilon — but still assumes rows are of similar magnitude.
- Consequences: fewer rounding-error pivots, better sparsity retention, less stalling on degenerate rows.
- Verification must independently re-check feasibility, because the band deliberately allows a slightly-larger step.

## Related
- [[Degeneracy]]
- [[Numerical Stability]]
- [[Scaling]]
- [[Dual Simplex]]
- [[Harris-1973-Pivot-Selection-Methods]]
- [[Koberstein-2005-Dual-Simplex-Method]]

## Referenced By

- [[21-traceability|audit/21-traceability]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Revised Simplex|research/algorithms/Revised Simplex]]
- [[Degeneracy|research/concepts/Degeneracy]]
- [[Numerical Stability|research/concepts/Numerical Stability]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[Benichou-1997-Linear-Programming-Implementations|research/papers/Benichou-1997-Linear-Programming-Implementations]]
- [[DeFarias-2019-Positive-Edge-Pricing|research/papers/DeFarias-2019-Positive-Edge-Pricing]]
- [[Georg-1987-Numerical-Stability-Simplex|research/papers/Georg-1987-Numerical-Stability-Simplex]]
- [[Gill-1989-Practical-Anti-Cycling|research/papers/Gill-1989-Practical-Anti-Cycling]]
- [[Harris-1973-Pivot-Selection-Methods|research/papers/Harris-1973-Pivot-Selection-Methods]]
- [[Koberstein-2005-Dual-Simplex-Method|research/papers/Koberstein-2005-Dual-Simplex-Method]]
- [[Maros-1993-Practical-Anti-Degeneracy|research/papers/Maros-1993-Practical-Anti-Degeneracy]]
- [[Maros-2003-Generalized-Dual-Phase|research/papers/Maros-2003-Generalized-Dual-Phase]]
- [[Degeneracy Handling Gap|research/research-gaps/Degeneracy Handling Gap]]
- [[Bland Anti-Cycling|research/techniques/Bland Anti-Cycling]]