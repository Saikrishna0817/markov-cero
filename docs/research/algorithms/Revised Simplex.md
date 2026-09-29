---
type: concept
tags: [algorithms, lp]
status: stable
verified_on: 2026-09-25
---

# Revised Simplex

> **Research snapshot:** This note records an earlier literature/code reading. Check the [current capability register](../../project/STATUS.md) and [dated evidence](../../../evidence/INDEX.md) before treating its implementation or performance statements as current.


> The simplex that never forms B⁻¹ — factor the basis, solve, pivot — and the LP method R4 names first.

## Definition
The revised simplex maintains a basis factorization instead of the full tableau: each iteration prices columns (d = c − πᵀA) to pick an entering column, computes the tableau column ȳ = B⁻¹A_j by a forward solve, runs the ratio test to pick a leaving row, then rank-one updates the factorization. Cost per iteration is O(nnz of touched columns + factor solve) rather than O(mn) for the full tableau, so it is the sparse-friendly formulation. Two-phase variants add Phase-I artificials to reach a basic feasible solution, and a positive Phase-I optimum is reinterpretable as a Farkas infeasibility certificate.

## Why It Matters Here
- R4 names revised simplex explicitly; it is the certified reference path and the fallback for cold starts.
- Observed state: two-phase certified engine at src/lp/reference/revised_simplex.cpp:383 with caps 1024 rows / 8192 cols / 4M elements / 1e6 iterations; Bland pricing default; eta updates up to 64 before refactorization; every terminal result passes `certify` (src/lp/reference/revised_simplex.cpp:357).
- Inference: the dense workspace and dimension caps are why the module is called "reference" — production-scale instances must reach the sparse dual/first-order engines.

## Key Facts / Rules
- Pricing: dⱼ = cⱼ − πᵀAⱼ; enter on dⱼ < 0 (rule chooses which).
- Ratio test: θ = min over rows with ȳᵢ > 0 of (basic value / ȳᵢ), with tolerance handling (see [[Harris Ratio Test]]).
- FTRAN solves Bx = rhs; BTRAN solves Bᵀπ = c_B; updates are eta vectors, refactored on density/count triggers.
- Phase-I objective > 0 ⇒ infeasible, reported as a certificate rather than a failure.

## Related
- [[Basis]]
- [[Reduced Cost]]
- [[Dual Simplex]]
- [[Bland Anti-Cycling]]
- RevisedSimplexEngine
- [[Dantzig-1963-Linear-Programming-Extensions]]

## Referenced By

- Canonicalizer
- IndependentVerifiers
- RevisedSimplexEngine
- Algorithms MOC
- Architecture MOC
- Research MOC
- [[Dual Simplex|research/algorithms/Dual Simplex]]
- [[Basic Solution|research/concepts/Basic Solution]]
- [[Basis|research/concepts/Basis]]
- [[ED-002-keep-simplex-core-add-first-order-not-replace|research/engineering-decisions/ED-002-keep-simplex-core-add-first-order-not-replace]]
- cross-paper-synthesis
- [[Azulay-0000-Revised-Simplex-Method|research/papers/Azulay-0000-Revised-Simplex-Method]]
- [[Bartels-1968-Numerical-Investigation-Simplex|research/papers/Bartels-1968-Numerical-Investigation-Simplex]]
- [[Bland-1977-Anti-Cycling-Rule|research/papers/Bland-1977-Anti-Cycling-Rule]]
- [[Dantzig-1963-Linear-Programming-Extensions|research/papers/Dantzig-1963-Linear-Programming-Extensions]]
- [[Gartner-1999-Exact-Arithmetic-Low|research/papers/Gartner-1999-Exact-Arithmetic-Low]]
- [[Goldfarb-1977-Practicable-Steepest-Edge|research/papers/Goldfarb-1977-Practicable-Steepest-Edge]]
- [[Hall-2005-Hyper-sparsity-Revised-Simplex|research/papers/Hall-2005-Hyper-sparsity-Revised-Simplex]]
- [[Huangfu-2018-Parallelizing-Dual-Revised|research/papers/Huangfu-2018-Parallelizing-Dual-Revised]]
- [[Mehlhorn-2010-Implementation-Simplex-Algorithm|research/papers/Mehlhorn-2010-Implementation-Simplex-Algorithm]]
- [[Bland Anti-Cycling|research/techniques/Bland Anti-Cycling]]