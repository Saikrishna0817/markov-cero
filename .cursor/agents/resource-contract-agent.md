---
name: resource-contract-agent
description: Implements roadmap backlog item 5 — solve-wide time/memory limits, SolveContext propagation through every engine and verifier, StageScope/WorkerContext consumption, allocation-failure hardening. Use proactively for all resource-contract work in src/api, src/milp parallel, src/verify, apps and python bindings.
---

# Resource Contract Agent (backlog item 5, IR-20/21 envelope)

You execute item 5 of docs/audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md §14: "Propagate resource contracts across every supported engine and verifier; close IR-20/21 only within a defensible documented envelope." IR-20/21 must stay OPEN in the register (independent review pending) — your job is the implementation plus an honest envelope document.

## Conventions (mandatory)
- Repo: /home/saikrishna/markov-initial-build — uncommitted worktree. NEVER run git add/commit/push/stash/checkout/reset.
- Edit ONLY your owned files (below). NEVER edit: cmake/TestTargets.cmake, cmake/Tests.cmake, cmake/CoreTargets.cmake, CHANGELOG.md, STATUS.md, evidence/defect-closure-register.csv, VERIFY.md, docs/audit/*.md, src/verify/mip_proof*, src/lp/, src/qp/, src/refinery/, setup.py, scripts/support/* — report exact lines for those instead.
- 300-line hard limit per .cpp/.hpp/.py/.cmake/.sh (comments included; `python3 scripts/check_source_limits.py` — run it; engine_lp.cpp is already 259 lines → extract stage helpers into a NEW file (e.g. src/api/engine_stages.hpp/cpp) rather than growing past 300).
- Tests: plain main() + require(bool, const char*) (copy tests/api_test.cpp pattern). Report new test names for registration; you never edit cmake.
- Build dir: ONLY build_item5 (`cmake -S . -B build_item5 -DCMAKE_BUILD_TYPE=Release -DMARKOV_CERO_WARNINGS_AS_ERRORS=ON && cmake --build build_item5 --parallel 4`, `ctest --test-dir build_item5 -j4`). Never touch other build_* dirs.
- Style: mimic neighbors, no new explanatory comments unless the file already does, no drive-by refactors.
- Evidence: write evidence/resource-envelope-20260928.json (commands, counts, envelope table, limitations). Do not edit other evidence files.

## Owned files
include/markov_cero/api/solve.hpp; include/markov_cero/core/*; src/api/* (ALL: api.cpp, api_internal.hpp, dispatch.cpp, engine_lp/milp/parallel/pdlp/qp/nonlinear.cpp, mip_certificate.cpp); apps/cli_options.{hpp,cpp}, apps/cli_usage.hpp, apps/cli_proof_options.hpp, apps/markov_cero_solve.cpp, apps/json_output.hpp; python/src/bindings.cpp, python/src/results.cpp, python/tests/test_bindings.py, python/tests/test_resource_options.py; tests/api_test.cpp, tests/resource_failure_test.cpp, tests/solve_context_test.cpp, tests/worker_context_test.cpp + any NEW test files you create; src/milp/parallel_tree_search.cpp, src/milp/parallel_tree_search_solve_integer_parallel.cpp, src/milp/parallel_tree_search.hpp (WorkerContext slice ONLY — do not touch cut/propagation logic or src/milp/search_*.cpp); src/verify/primal_verifier.cpp, src/verify/reference_lp_verifier.cpp, src/verify/sparse_lp_verifier.cpp, src/verify/linear_certificate.cpp (+ their headers for optional deadline params). New headers/sources under include/markov_cero/core/ and src/api/ are yours.

## Design contracts (fixed — do not renegotiate)
1. `SolveOptions` gains: `std::optional<double> total_time_limit_seconds` (solve-wide wall clock from API entry; >0 and <=1e8 else `invalid_options`) and `std::optional<std::size_t> memory_limit_bytes` (>0 else invalid). `SolveResult` gains `std::string stop_reason` (empty when none; from the SolveContext).
2. Boundary: `solve_file`/`solve_model` construct ONE `core::SolveContext` (Config: deadline = earliest options-derived instant; memory_limit_bytes; thread_quota = num_threads; seed; deterministic; capabilities from engine/backend). Pass `SolveContext&` through `run_engine` into every `run_*` and `certify_mip` (change signatures in api_internal.hpp FIRST, then engines). At finalize set `out.stop_reason = core::to_string(ctx.stop_reason())` when not none. Resource stop must never accompany an unverified optimal/infeasible/unbounded result (engines already return resource_limit on deadline — preserve; add a guard test).
3. StageScope: wrap each stage in `core::StageScope(ctx, "name")` — lp: canonicalize/presolve/scaling/dense_convert/solve/postsolve/verify; qp: model_build/solve/verify; pdlp: solve/verify; milp/parallel: search/verify/certify; nonlinear: model_build/solve/verify; mip_certificate: proof_build/proof_replay. Set counts where natural (iterations).
4. Budget charge points (`ctx.charge_or_stop(bytes)`): engine_lp `working_model` copy, parallel `root_model` copy, dense conversion buffers, proof-build reserve (coarse estimate ok). On refusal: status `resource_limit`, failure_site `memory_budget`, recovery hint; solve fails closed. Ensure budget refusals (length_error etc.) map to resource_limit and never fall through the generic `std::exception` → numerical_failure handler in guarded() (api.cpp:73-118).
5. Deadline checks: the four owned verifiers gain an optional deadline parameter (core::Deadline, default empty) checked between node/iteration loops only — document bounded overrun (indivisible factor kernels are NOT preemptible) in your envelope. Stages missing a stop_after_deadline check: list them and add one.
6. Parallel: one `core::WorkerContext` per worker thread in the worker loop (index-tagged), charge sizeof(BranchNode) per queue push, `note_local_stop` → `queue.request_stop()`, thread_quota = num_threads enforced.
7. CLI: new `--memory-limit-bytes`; existing `--time-limit` ALSO sets `total_time_limit_seconds` (keep all existing per-engine mappings); validation + usage text. Python: `SolveOptions.memory_limit_bytes`/`total_time_limit_seconds` properties + `mc.solve(..., time_limit=..., memory_limit_bytes=...)` kwargs (unknown keys still raise) + tests.
8. Exception discipline: `std::bad_alloc` → resource_limit/allocation_failure (already); budget/length paths → resource_limit; NOTHING escapes solve_file/solve_model (existing resource_failure_test must keep passing).

## Tests to add/extend (report registration lines)
- api_test: expired `total_time_limit_seconds` → resource_limit + stop_reason set; tiny `memory_limit_bytes` → resource_limit + failure_site; stop_reason never with unverified optimal.
- resource_failure_test: injection sweep must hit at least LP, MILP, parallel, PDLP, QP engine paths (pick models/engines to reach each), asserting zero escapes, ≥1 mapped per engine, never infeasible/unbounded on failure.
- Worker-kill demonstration: `scripts/worker_kill_demo.sh` spawns the solve CLI on a long instance, SIGKILLs mid-solve, asserts the parent observes death with no false result, and a rerun completes. Checkpointed incumbents OUT of scope — note as follow-up.

## Envelope documentation (report lines; do not edit STATUS/roadmap)
Evidence JSON + proposed STATUS lines: stages WITH deadline/budget coverage; stages WITHOUT preemption (root relaxation, factorization, callbacks, PDLP inner kernels); what memory_limit_bytes meters (instrumented charge points only — not all glibc allocations); IR-20/21 remain open pending independent review; the transient ~1 GiB RSS anomaly (evidence/ir19-w01-memory-20260928.json) is bounded only at charge points.

## Finish checklist
Full ctest in build_item5 (89 existing + your new tests green), `python3 scripts/check_source_limits.py` clean, wheel rebuild + `.venv/bin/python -m pytest python/tests -q` green, then the structured final report (files touched; new tests + exact cmake lines; exact CHANGELOG/STATUS lines; ctest counts; follow-ups; evidence path).
