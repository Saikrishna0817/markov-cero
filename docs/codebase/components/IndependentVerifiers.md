---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "src/verify/reference_lp_verifier.cpp:53"
  - "src/verify/primal_verifier.cpp:47"
  - "src/qp/verifier.cpp:46"
verified_on: 2026-09-25
---

# IndependentVerifiers

> Three independent checkers: canonical KKT/Farkas verifier, original-model primal verifier, and QP KKT-certificate verifier.

## Responsibility
- Re-derive optimality/infeasibility claims from raw vectors so that a solver output is only reported `verified` when an independent recomputation agrees.

## Implementation Facts (Observed)
- **Canonical verifier** `verify_reference_result(CanonicalModel, Result, tol)` (src/verify/reference_lp_verifier.cpp:53, decl include/markov_cero/verify/reference_lp_verifier.hpp:12): tolerance must be in (0, 1e-4] (src/verify/reference_lp_verifier.cpp:60-63); for `optimal` it checks primal, dual and complementarity with allowance `tol*scale + 512ε·max(1,scale)` (src/verify/reference_lp_verifier.cpp:27-28, 72-109); for `infeasible` it validates the Farkas certificate with a positive-margin requirement (src/verify/reference_lp_verifier.cpp:114-130); for `unbounded` it validates the improving ray (src/verify/reference_lp_verifier.cpp:144-166).
- Both simplex engines call it in `certify` — failure downgrades status to `numerical_failure` with message "internal witness verification failed" (src/lp/reference/revised_simplex.cpp:354-363; src/lp/dual/dual_simplex.cpp:257-278).
- **Original-model primal verifier** `verify_primal(model, Candidate, feas_tol, obj_tol, integrality_tol)` (src/verify/primal_verifier.cpp:44, decl include/markov_cero/verify/primal_verifier.hpp:41-45): recomputes objective, row/variable/integrality violations, appends `Violation{category,index,actual,bound,magnitude,allowance}`, passes iff `violations.empty()` (src/verify/primal_verifier.cpp:73-101).
- **QP verifier** `verify_qp_solution(model, solution, tolerance=1e-4)` (src/qp/verifier.cpp, decl include/markov_cero/qp/verifier.hpp:23-26): tracks max primal/dual/complementarity/integrality violation and objective discrepancy, with a concatenated `failure_reason` (src/qp/verifier.cpp:87-88, 123-124, 153-171).
- CLI wiring: primal verifier for every engine output (apps/markov_cero_solve.cpp:119, 168, 257, 397); QP verifier for `qp` (apps/markov_cero_solve.cpp:211-219, message `"QP KKT certificate verified"` at :214); canonical check re-derived inline for the sparse pipeline (apps/markov_cero_solve.cpp:353-389).
- Final `verified` flag = optimal ∧ original ∧ canonical, or infeasible/unbounded ∧ canonical (apps/markov_cero_solve.cpp:439-443).

## Dependencies
- [[Canonicalizer]], [[DenseLU]], [[RevisedSimplexEngine]] (shared `SolveStatus`)

## Used By
- [[RevisedSimplexEngine]], [[DualSimplexEngine]], [[QP-ADMM-Engine]], [[Solve-Pipeline]], [[Solution-JSON-Writer]]

## Research Justification
- [[Revised Simplex]], [[Dual Simplex]] (certificate forms)

## Open Questions / Risks
- PDLP results are checked only with the primal verifier plus a scaled tolerance, never a dual/certificate check (apps/markov_cero_solve.cpp:164-171).
- MILP/parallel branches set `canonical_verified = true` without an explicit canonical re-check (apps/markov_cero_solve.cpp:121, 259) — Inference: they rely on the primal verifier alone.
