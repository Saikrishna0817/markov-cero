---
type: research-gap
tags: [research-gaps, lp]
status: addressed
verified_on: 2026-09-25
resolved_on: 2026-09-26
---

# No Interior-Point Engine

> R4 names interior-point methods alongside revised simplex; the repo has a first-order method instead — the hardest structural gap.

## Definition
The gap is the absence of a barrier/path-following LP (and QP) engine: no predictor-corrector iteration, no central-path parameter, no KKT factorization for LP, and consequently no crossover or basis from that path. PDHG/PDLP shares the "matrix-free, GPU-friendly" niche but belongs to a different algorithm class with different accuracy and iteration profiles; calling it interior-point would be a category error. Closing the gap means implementing a primal-dual IPM (Mehrotra-style) on the existing sparse KKT machinery plus crossover into the existing dual simplex.

## Why It Matters Here
- R4 is explicit ("revised simplex and interior-point methods"); PS-GAP-06 records the absence, and the divergence table classifies it as **GAP (hard)** (docs/audit/00-ground-truth.md, section B preview and A-vs-B-vs-C table).
- Observed state: LP paths are revised simplex, dual simplex, PDLP (first-order); QP is ADMM — none follow the barrier path (src/lp/*, src/qp/*).
- Inference: the reusable pieces exist — sparse KKT assembly (qp/kkt.cpp), LDLᵀ, presolve, scaling — so the gap is algorithmic, not infrastructural.

## Key Facts / Rules
- Required components: path-following/predictor-corrector iteration, augmented KKT factorization (LDLᵀ), centring/step-length rules, stopping on primal-dual residual + gap.
- Companion requirement: crossover to a basis, or the result cannot feed MIP node LPs (see [[No Crossover]]).
- Why it matters beyond compliance: IPM's iteration count is insensitive to degeneracy — a complement to simplex on the R13 cases, not a replacement.
- Verification: same independent KKT verification path applies; no new trust surface needed.

## Related
- [[Interior-Point Method]]
- [[Crossover]]
- [[No Crossover]]
- [[Sparse LDL Factorization]]
- [[Mehrotra-1992-Implementation-Primal-Dual-Interior]]
- [[Wright-1997-Primal-Dual-IPM]]

## Resolution (2026-09-26)

Closed by the AP-1 implementation:

- **Engine**: `src/lp/interior/ipm.cpp` — infeasible-start Mehrotra predictor-corrector on
  the canonical standard form (`Ax = b`, `x >= 0`), normal-equation Newton systems
  `(A D A^T) dy = rhs` with `D = diag(x_j/s_j)`, Ruiz-equilibrated before iteration, and a
  diagonal-perturbation fallback when the normal system fails to factorize (per
  [[Lustig-1992-Implementing-Mehrotras-Predictor-Corrector]]).
- **Crossover**: rank-revealing basis construction from the interior iterate (large-x columns
  first, independent-set test per candidate, rank-completion fallback) with certification
  delegated to the dual simplex via a warm start — see [[Crossover]] and
  [[Ye-1998-Crossover-Interior-Point]]. The canonical matrix has **no full slack identity
  block** (equality rows carry no slack column), so selection must be rank-revealing rather
  than "large-x plus slack padding".
- **Dispatch**: `--engine ipm` (CLI + `src/api/api.cpp`), with the documented engine fallback
  to the reference primal simplex when the IPM cannot certify, reported honestly in telemetry.
- **Evidence**: `tests/ipm_test.cpp` (vertex optimum, equality-row basis regression, basis
  reusable as a dual warm start, crossover-disabled path); Netlib sweep 16/17 optimal+verified
  with 8 crossover-certified vertices and 8 honest fallbacks.

## Referenced By

- [[18-risk-register|audit/18-risk-register]]
- [[21-traceability|audit/21-traceability]]
- [[Algorithms MOC|research/Algorithms MOC]]
- [[Architecture MOC|research/Architecture MOC]]
- [[Research MOC|research/Research MOC]]
- [[Research-Code Traceability MOC|research/Research-Code Traceability MOC]]
- [[Interior-Point Method|research/algorithms/Interior-Point Method]]
- [[ED-003-interior-point-required-by-ps|research/engineering-decisions/ED-003-interior-point-required-by-ps]]
- [[First-Order Accuracy Ceiling|research/limitations/First-Order Accuracy Ceiling]]
- [[No Crossover|research/limitations/No Crossover]]
- [[cross-paper-synthesis|research/maps/cross-paper-synthesis]]
- [[research-dependency-map|research/maps/research-dependency-map]]