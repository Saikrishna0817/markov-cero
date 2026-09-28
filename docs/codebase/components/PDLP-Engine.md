---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/lp/first_order/pdlp.cpp:173"
  - "include/markov_cero/lp/first_order/pdlp.hpp:64"
verified_on: 2026-09-25
---

# PDLP-Engine

> Matrix-free CPU/GPU Primal-Dual Hybrid Gradient (PDHG/PDLP) first-order LP engine.

## Responsibility
- Solve continuous LPs without any basis factorization, using only SpMV (`A*x`, `A^T*y`), ergodic averaging, adaptive restarts and adaptive primal weighting.

## Implementation Facts (Observed)
- `solve_pdlp(model, options)` declared at include/markov_cero/lp/first_order/pdlp.hpp:64; header cites Chambolle & Pock (2011) and Applegate et al. (2021) (include/markov_cero/lp/first_order/pdlp.hpp:3-5).
- Backend dispatch: `Backend::gpu` → `gpu::solve_pdlp_gpu`, else CPU loop (src/lp/first_order/pdlp.cpp:174-175).
- Optional Ruiz scaling inside the engine (`ruiz_scaling` default true, 10 iterations) (src/lp/first_order/pdlp.cpp:189-190, include/markov_cero/lp/first_order/pdlp.hpp:33-34).
- Step sizes `tau=(eta/omega)/col_norm`, `sigma=(eta*omega)/row_norm` from row/column 1-norms (src/lp/first_order/pdlp.cpp:238-244); initial weight `omega=sqrt(||c||inf/||b||inf)` clamped to [0.01,100] (src/lp/first_order/pdlp.cpp:232-234).
- Adaptive step size every 10 iterations estimates a local Lipschitz constant `L_local` and retargets `eta≈0.95/L` (src/lp/first_order/pdlp.cpp:288-307).
- Restart every `restart_every` (default 40) iterations: fixed or adaptive; adaptive fires when score ≤0.368× previous or after 5 intervals of improvement (src/lp/first_order/pdlp.cpp:327-390, include/markov_cero/lp/first_order/pdlp.hpp:20,28).
- Primal weight is updated on restart as `omega *= (sqrt(primal/dual))^0.5` clamped to [1e-6,1e6] (src/lp/first_order/pdlp.cpp:361-386).
- Termination returns `PdlpStatus::optimal` only when primal, dual and gap residuals all ≤ tolerance; otherwise `iteration_limit` (src/lp/first_order/pdlp.cpp:333-346, 404-411).
- CLI sets `max_iterations=100000` unless `--iteration-limit` is passed (apps/markov_cero_solve.cpp:141-145).

## Dependencies
- [[RuizScaling]], [[GPU-PDHG-Engine]], [[IndependentVerifiers]]

## Used By
- [[Solve-Pipeline]], [[CLI-MarkovCeroSolve]]

## Research Justification
- [[Primal-Dual Hybrid Gradient]]

## Open Questions / Risks
- `PdlpOptions::power_iterations` (include/markov_cero/lp/first_order/pdlp.hpp:25) is never read in src/gpu — UNVERIFIED whether power-iteration spectral estimation was intended.
- `PdlpStatus::infeasible_or_unbounded` (hpp:43) is never produced anywhere in src/gpu/apps — first-order engine cannot currently report infeasibility.
