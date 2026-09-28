---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "CMakeLists.txt:30"
  - "CMakeLists.txt:68"
verified_on: 2026-09-25
---

# Library-API

> Public C++20 surface: static lib `markov_cero_core` exposing headers under `include/markov_cero/` (+ `gpu/include/`).

## Responsibility
- Give embedders direct access to parsing, canonicalization, engines, presolve/scale, MILP, QP and verification, bypassing the CLI.

## Implementation Facts (Observed)
- Single static library target `markov_cero_core` listing all src + gpu sources (CMakeLists.txt:30-67).
- Public include dirs: `include` and `gpu/include`; `cxx_std_20`; links `Threads::Threads` (CMakeLists.txt:68-70).
- CUDA adds `MARKOV_CERO_HAS_CUDA` as a PUBLIC definition when `MARKOV_CERO_ENABLE_CUDA` is ON (default OFF) (CMakeLists.txt:7, 71-72).
- No `install()` / export rules exist in CMakeLists.txt (searched: no `install(` matches) — Inference: consumption is via `add_subdirectory`/path include, not an installed package.
- Namespaces and key entry points (all under `markov_cero::`):
  - `model` — `Model`, `Bound`, `VariableType`, `ObjectiveSense`, `SparseMatrixCSC`, `SparseMatrixBuilder` (include/markov_cero/model/model.hpp:9-73).
  - `io` — `parse_mps(std::istream&, MpsLimits = {})`, `parse_mps_string`, `MpsError`, `MpsLimits` (include/markov_cero/io/mps.hpp:11-33).
  - `transform` — `canonicalize`, `sparse_canonicalize`, `reconstruct_primal/objective`, `CanonicalModel`, `SparseCanonicalModel` (include/markov_cero/transform/canonicalize.hpp:24-27, sparse_canonical_model.hpp:27-32).
  - `presolve` — `presolve`, `postsolve`, `PresolveStack` (include/markov_cero/presolve/presolve.hpp:39-45).
  - `scale` — `equilibrate`, `equilibrate_model`, `unscale_solution`, `RuizScalers` (include/markov_cero/scale/ruiz_scaling.hpp:28-38).
  - `lp::reference` — `solve(model, Options)`, `SolveStatus`, `Result` (include/markov_cero/lp/reference/revised_simplex.hpp:7-50).
  - `lp::dual` — `solve(model, Options, warm_start)`, `BasisState`, `serialize_basis/parse_basis`, `make_basis_state` (include/markov_cero/lp/dual/dual_simplex.hpp:16-63).
  - `lp::first_order` — `solve_pdlp`, `PdlpOptions`, `Backend` (include/markov_cero/lp/first_order/pdlp.hpp:15-64).
  - `linalg` — `DenseLu`, `SparseLu`, `SparseBasisFactorization`, `SparseCsc` (include/markov_cero/linalg/dense_lu.hpp:19, sparse_basis.hpp:25-89).
  - `milp` — `solve`, `solve_parallel`, `Options`, `ParallelOptions` (include/markov_cero/milp/milp_solver.hpp:48, parallel_tree_search.hpp:28).
  - `qp` — `solve_qp`, `make_quadratic_model`, `KktSolver`, `verify_qp_solution` (include/markov_cero/qp/admm_solver.hpp:63, model, kkt.hpp:18, verifier.hpp:23).
  - `verify` — `verify_primal`, `verify_reference_result` (include/markov_cero/verify/primal_verifier.hpp:41, reference_lp_verifier.hpp:12).
  - `foundation` — `version()`, `milestone()` (include/markov_cero/foundation/build_info.hpp:3).
  - `gpu` — `solve_pdlp_gpu`, `PdhgState`, `DeviceCsr`, `DeviceBuffer` (gpu/include/markov_cero/gpu/pdhg_step.hpp:98, csr.hpp, buffer.hpp).
- Result/status vocabulary is shared: most engines return or consume `lp::reference::SolveStatus`/`Result` (e.g. `presolve::PresolveResult` embeds it, include/markov_cero/presolve/presolve.hpp:32; `milp::Result` too, include/markov_cero/milp/milp_solver.hpp:35).

## Dependencies
- Aggregate of the component and data-flow notes in this folder (no single root module).

## Used By
- [[CLI-MarkovCeroSolve]], [[CLI-InfoAndMpsInspect]], all test targets (CMakeLists.txt:104+)

## Research Justification
- (library surface; no research note)

## Open Questions / Risks
- Header-only JSON/CLI helpers live in `apps/` (apps/cli_options.hpp, apps/json_output.hpp) and are **not** part of the public library — Inference: library consumers must format output themselves.
- No ABI/version guard or package config file (no install/export rules observed).
