---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "apps/json_output.hpp:127"
  - "apps/markov_cero_solve.cpp:445"
verified_on: 2026-09-25
---

# Solution-JSON-Writer

> Hand-rolled JSON emitter for solve results, printed to stdout and optionally written via `--output`.

## Responsibility
- Serialize status, verification flags, residuals, tree metrics and GPU timing into a single-line JSON object, and map solve status to process exit codes.

## Implementation Facts (Observed)
- Single header `apps/json_output.hpp` (no separate .cpp); struct `JsonOutputData` carries all fields (apps/json_output.hpp:91-125), populated at apps/markov_cero_solve.cpp:445-479 and emitted by `emit_json_output` (apps/json_output.hpp:127-200).
- Output keys include `version, milestone, engine, status, rows, cols, nonzeros, verified, message, objective, primal, canonical_verified, original_verified, original_message, used_warm_start, used_cold_fallback, maximum_primal_violation, maximum_variable_violation, maximum_integrality_violation, maximum_canonical_primal_violation, maximum_canonical_dual_violation, runtime_ms, nodes_explored, lp_iterations, best_bound, relative_gap, cuts_generated, heuristics_found, phase_one_iterations, phase_two_iterations, pdlp_tolerance, relative_primal_residual, relative_dual_residual, relative_duality_gap, backend, h2d_ms, kernel_ms, d2h_ms, total_ms, limitations` (apps/json_output.hpp:129-185).
- `objective` prefers `original_objective` when status is optimal, else the engine result (apps/json_output.hpp:140-143); `primal` prefers `original_primal` when non-empty (apps/json_output.hpp:145-147).
- `total_ms` reports PDLP wall time for the `pdlp` engine and overall elapsed time otherwise (apps/json_output.hpp:181-183).
- Non-finite doubles serialize as `null` (`json_number`, apps/json_output.hpp:69-78); strings escaped incl. control characters as `\u00XX` (`json_escape`, apps/json_output.hpp:45-67).
- Emission: always to `std::cout`, and additionally to `--output FILE` when set (failure prints `cannot write output`) (apps/json_output.hpp:190-199).
- Status→exit-code map in `exit_code()`: optimal 0, infeasible 1, unbounded 2, invalid_model 3, invalid_options 4, resource_limit 5, iteration_limit 6, numerical_failure 7 (apps/json_output.hpp:19-32); CLI returns this code (apps/markov_cero_solve.cpp:498) and also prints a human status line to stderr (apps/markov_cero_solve.cpp:492-497).

## Dependencies
- [[IndependentVerifiers]] (report structs are serialized), [[MPSParser]] (row/col/nnz counts)

## Used By
- [[CLI-MarkovCeroSolve]] — sole writer of the solution JSON

## Research Justification
- (output contract; no research note)

## Open Questions / Risks
- JSON is built by string concatenation with no schema validation; unknown/absent optional fields (e.g. `error`) are appended conditionally (apps/json_output.hpp:186-188).
- Inference: consumers parse `verified`/`status`; any key rename would be a breaking contract change (no versioned schema found — UNVERIFIED).
