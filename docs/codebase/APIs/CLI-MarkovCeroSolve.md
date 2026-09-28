---
type: codebase-component
tags: [codebase, component]
status: verified
source_files:
  - "apps/markov_cero_solve.cpp:35"
  - "apps/cli_options.hpp:63"
verified_on: 2026-09-28
---

# CLI-MarkovCeroSolve

> `markov-cero-solve MODEL.mps [options]` — the primary solver command line.

## Responsibility
- Parse flags into engine/back-end/limit options, run [[Solve-Pipeline]], print JSON, exit with a status code.

## Implementation Facts (Observed)
- Target `markov-cero-solve` from `apps/markov_cero_solve.cpp` (CMakeLists.txt:101-102); flag parsing in `apps/cli_options.hpp::CliOptions::parse` (apps/cli_options.hpp:63).
- Actual flags (usage text apps/cli_options.hpp:34-61, confirmed in `parse`):
  `--output FILE`, `--engine primal|dual|pdlp|milp|parallel|qp|miqp|auto`, `--threads N`, `--branching most_fractional|pseudo_cost|strong_branching|reliability`, `--iteration-limit N`, `--max-nodes N`, `--max-queued-nodes N`, `--max-input-bytes N`, `--time-limit SEC`, `--cuts`/`--no-cuts`, `--heuristics`/`--no-heuristics`, `--warm-start FILE`, `--save-basis FILE`, `--presolve`/`--no-presolve`, `--scale`/`--no-scale`, `--max-presolve-passes N`, `--ruiz-iterations N`, `--tolerance TOL`, `--backend cpu|gpu`, `--help`/`-h`.

`--max-input-bytes` overrides the parser byte cap for the selected MPS or LP file and must be in `1..1,073,741,824`. When omitted, the API keeps its format-specific defaults (256 MiB for MPS and 16 MiB for LP).
- Defaults: `engine=auto`, `threads=4`, presolve on, scale on, `max_presolve_passes=5`, `ruiz_iterations=10`, `pdlp_tolerance=1e-4`, `backend=cpu` (apps/cli_options.hpp:16-25).
- Engine token validation rejects unknown values with exit code 8 (apps/cli_options.hpp:86-93); `--backend` restricted to `cpu`/`gpu` (apps/cli_options.hpp:224-227); `--tolerance` must be > 0 (apps/cli_options.hpp:212-215).
- `--branching` maps to `milp::BranchingStrategy` incl. `reliability` (apps/cli_options.hpp:110-119); `--iteration-limit` sets both `reference::Options::iteration_limit` and `milp::Options::max_iterations` (apps/cli_options.hpp:242-243).
- Back-end selection only affects PDLP: `pdlp_opts.backend = gpu if cli.backend_name == "gpu"` (apps/markov_cero_solve.cpp:138-140).
- PDLP iteration default: CLI uses 100000 unless `--iteration-limit` differs from the 10000 sentinel (apps/markov_cero_solve.cpp:141-145).
- Basis I/O: `--warm-start` parsed by `lp::dual::parse_basis`, `--save-basis` written via `serialize_basis` (apps/markov_cero_solve.cpp:316-324, 404-409).
- Exit codes: 0 optimal, 1 infeasible, 2 unbounded, 3 invalid_model, 4 invalid_options, 5 resource_limit, 6 iteration_limit, 7 numerical_failure (apps/json_output.hpp:19-32); usage/parse errors exit 8 (apps/cli_options.hpp:75, 91, 248).
- Stderr one-line summary ends with `VERIFIED` / `NOT VERIFIED` (apps/markov_cero_solve.cpp:492-497).

## Dependencies
- [[Solve-Pipeline]], [[Solution-JSON-Writer]], [[MPSParser]]

## Used By
- benchmark/test harnesses (`scripts/run_netlib.py`, `scripts/run_miplib.py` referenced in CMakeLists.txt:213-214)

## Research Justification
- (no research note; CLI surface)

## Open Questions / Risks
- No `--seed`, `--mip-gap`, `--cut-rounds` or `--pump-iterations` flags: those knobs exist only in `milp::Options` defaults (include/markov_cero/milp/milp_solver.hpp:16-31) — Inference: not tunable from the CLI.
- Mixed argv forms (long-only) are supported; no short options besides `-h` (apps/cli_options.hpp:67).
