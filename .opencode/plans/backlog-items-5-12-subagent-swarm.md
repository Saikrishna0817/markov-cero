# Execution plan: backlog items 5–12 via parallel subagent swarm

Status: READY TO LAUNCH. User approved: single tree + ownership partition; items 10/11 do code+docs with gates open; maximum parallelism, finish ASAP (user must complete ALL items in the implementation plan).

Blocker: session is in plan mode — `edit` permission denies all files except `.opencode/plans/*.md`. On approval (plan mode off / edits allowed), execute this document verbatim: (A) create the 8 agent files under `.cursor/agents/` from the briefs in §4, (B) launch Wave 1 tasks in ONE message.

## 1. Research basis (already completed, do not re-survey)

- Item 5 survey: `SolveOptions` (solve.hpp:21-46) has NO solve-wide time/memory field; boundary creates no `SolveContext`; zero production uses of `SolveContext/StageScope/WorkerContext`; engines take `(const Model&, const SolveOptions&, SolveResult&, Result&)` (api_internal.hpp:35-42); `guarded()` catch map in api.cpp:73-118 (bad_alloc→resource_limit/allocation_failure, std::exception→numerical_failure); deadline plumbing is per-engine ad hoc (`lp_options.deadline` copies); verifiers have no deadline except mip_proof path; W02 queue cap = `milp_options.max_queued_nodes` enforced in search_*.cpp; `StopReason::queue_capacity_exhausted` never produced; CLI `--time-limit` maps to lp/milp/proof separately; Python kwargs whitelist (results.cpp:66-70) has no time/memory kwargs; tests: solve_context (267L), resource_failure (283L, injection harness), api_test (219L, expired-deadline + input-byte cases).
- Items 6-11 conflict matrix: cluster A (`solve.hpp`+`src/api`+`apps`+`python/src`) = items 5 then 6 → serialize; cluster B (`src/milp` dirty set) = items 5 then 9 → serialize; `src/verify/mip_proof*` shared 6 then 9 → serialize; clean parallel-safe: 7 (scripts/evidence), 8 (src/lp,src/qp), 10 (src/refinery), 11 (setup.py,cmake/Install,docs/governance). Universal shared writes: CHANGELOG.md, STATUS.md, cmake/TestTargets.cmake, cmake/Tests.cmake, evidence/defect-closure-register.csv, VERIFY.md → integrator (main agent) applies reported lines.
- Item 6 gaps: budgets+split timing exist (solve.hpp:36-39, mip_certificate.cpp:19-44); missing richer guarantee fields/assurance tier, proof metadata (mip_proof.hpp:16-35 has no budget/tier/version/fingerprint), exhausted-vs-rejected distinction, large-case timing evidence.
- Item 7 gaps: no frozen comparator versions (run_full_compare_annotate.py:16-70 probes only), hardcoded manifests (config.py:25-54, benchmark_config.py:11-45), only ML split exists (ml_splits.py:1-40), timing prose not code, no current frozen baseline (existing CSVs quarantined). Commands: `scripts/run_full_benchmark.py`, `scripts/run_full_compare.py`.
- Item 8 gaps: warm start = file-based only, no session reuse; QP factorizes per solve (admm_solver_solve.cpp:66, refactors only on rho :211-234); no CPU profiling path; src/lp+src/qp clean in git status.
- Item 9 gaps: proof is cut-free by design (mip_proof.hpp:37); `BoundEvidenceSource::propagated` has no producer; parallel cuts root-only (parallel_tree_search_root_cuts.cpp:4-45 with catch(...)); serial in-tree cuts exist (search_separate_cuts.cpp). Overlaps item-5 WorkerContext files → after item 5.
- Item 10 gaps: quality fields exist but sulfur/cetane/RVP hard-rejected (refinery_model.cpp:29-30); no units schema; no named refinery reports; refinery_test.cpp:10-71 exists. IR-34 engineer/shadow-trial gates EXTERNAL — code only, gates stay open.
- Item 11 gaps: packaging minimal (setup.py 48L, cmake/Install.cmake 18L); zero rollback docs; no support doc; verify-release.sh local-only. IR-33 external — code only, gates stay open.
- Verification matrix for the end: Release/ASan/TSan CTest (89 baseline + new tests), wheel + pytest (20), check_source_limits (449 files→grows), check_docs, check_json.

## 2. Hard rules for every agent (embed in each prompt)

- Repo /home/saikrishna/markov-initial-build, uncommitted; NEVER git add/commit/push/stash/checkout/reset.
- Edit ONLY owned files. NEVER edit: cmake/TestTargets.cmake, cmake/Tests.cmake, cmake/CoreTargets.cmake, CHANGELOG.md, STATUS.md, evidence/defect-closure-register.csv, VERIFY.md, docs/audit/*.md — report exact lines instead.
- 300-line limit per .cpp/.hpp/.py/.cmake/.sh (`python3 scripts/check_source_limits.py`).
- Tests: plain main() + require(bool,const char*) (copy neighbors); report registration lines; never edit cmake.
- Own build dir only (build_itemN); Release + -DMARKOV_CERO_WARNINGS_AS_ERRORS=ON; ctest -j4; never touch other build_* dirs.
- Own evidence JSON `evidence/<topic>-20260928.json`; never edit others'.
- Final report: files touched; new tests + exact cmake lines; exact CHANGELOG/STATUS lines; ctest/check results; follow-ups; evidence path.

## 3. Wave structure (critical path 5→6→9→12; compress by running 7 agents in Wave 1)

Wave 1 (parallel, one message, 7 tasks):
| Agent | Item | Owns | Build dir |
|---|---|---|---|
| resource-contract | 5 | solve.hpp, core/*, src/api/**, apps cli/json/solve, python/src+tests, tests/api+resource_failure, src/milp parallel slice only (parallel_tree_search*.cpp/.hpp, NOT search_*), src/verify primal/reference/sparse/linear (+headers) | build_item5 |
| baseline-freeze | 7 | scripts/support/*, scripts/run_full_*, scripts/datasets.py, scripts/ml/ml_splits.py, new manifests, evidence/benchmarks+comparison new dirs | none (py) |
| repeated-solve | 8 | src/lp/**, src/qp/**, include lp/qp, warm_start/dual/qp tests, scripts/bench_repeated_solve.py | build_item8 |
| refinery-schema | 10 | src/refinery/**, include refinery, refinery tests, scripts/generators gen_*refinery*, apps refinery/iis | build_item10 |
| packaging-release | 11 | setup.py, pyproject.toml, MANIFEST.in, cmake/Install.cmake, cmake/markov_ceroConfig.cmake.in, scripts/verify-release.sh, BUILDING.md, docs/governance/support-rollback.md (NEW) | build_item11 (install test) |
| proof-guarantee-p1 | 6a | src/verify/mip_proof*.cpp+hpp, tests/mip_proof_test.cpp, tests/repository_tools_test.py, proof evidence (NOT src/api, NOT solve.hpp, NOT apps/json_output) | build_item6 |
| (none — 6a/9 wait) | | | |

Wave 2 (after resource-contract reports done): 
- proof-guarantee-p2 (6b): solve.hpp guarantee fields + mip_certificate wiring + apps/json_output + python/src/results + tests (files freed by agent 5).
- milp-strengthen (9): src/milp/** (search_separate_cuts, search_finish, search_context, node relaxation/branch, gomory/mir/cover/cut_pool, parallel root cuts) + proof-obligation hookup in src/verify/mip_proof* (freed by 6a) — wait for BOTH 5 and 6a.

Wave 3 (integration, main agent): apply all reported cmake/CHANGELOG/STATUS/register/VERIFY lines; repeated-solve agent's engine hook-ups (engine_lp/qp one-liners) if deferred; full verification matrix (Release/ASan/TSan, wheel+pytest, source-limits/docs/json); item 12 (promotion/limitations doc pass via evidence-docs agent); final honest status: IR-19 closed; IR-20/21 open-with-envelope; IR-33/34 open (external); IR-28/35 unchanged.

Parallel-safety notes: repeated-solve never touches src/api (engine hook = header-declared function + reported one-line call); refinery/packaging/baseline have zero overlap with item 5; 6a touches only src/verify/mip_proof* + proof tests while 5 touches other verifiers (primal/reference/sparse/linear) — no shared files; json_output.hpp owned by 5 in Wave 1 (report only), transferred to 6b in Wave 2.

## 4. Agent files to create (`.cursor/agents/<name>.md`, YAML frontmatter name+description, body = system prompt)

Briefs drafted (content preserved from pre-block attempts — recreate verbatim):
1. `resource-contract-agent.md` — item 5 brief (see §5 below for the full body).
2. `baseline-freeze-agent.md` — item 7 brief (§6).
3. `repeated-solve-agent.md` — item 8 brief (§7).
4. `refinery-schema-agent.md` — item 10 brief (§8).
5. `packaging-release-agent.md` — item 11 brief (§9).
6. `proof-guarantee-agent.md` — item 6 brief split p1/p2 (§10).
7. `milp-strengthen-agent.md` — item 9 brief (§11).
8. `evidence-docs-agent.md` — item 12 + integration brief (§12).

Invocation in this session: Task tool, subagent_type `general`, prompt = "Read .cursor/agents/<name>.md and execute it fully. <wave-specific additions>". Launch all Wave-1 tasks in ONE message for concurrency.

## 5. resource-contract-agent brief (item 5)

Execute roadmap §14 item 5. IR-20/21 stay OPEN (independent review pending); deliver implementation + honest envelope.
Owned: solve.hpp; core/*; src/api/** (api.cpp, api_internal.hpp, dispatch.cpp, engine_lp/milp/parallel/pdlp/qp/nonlinear.cpp, mip_certificate.cpp); apps/cli_options.*, cli_usage.hpp, cli_proof_options.hpp, markov_cero_solve.cpp, json_output.hpp; python/src/bindings.cpp, results.cpp, python/tests/*; tests/api_test.cpp, resource_failure_test.cpp, solve_context_test.cpp, worker_context_test.cpp + new tests; src/milp/parallel_tree_search.cpp + parallel_tree_search_solve_integer_parallel.cpp + parallel_tree_search.hpp (WorkerContext slice ONLY, no cut logic, no search_*.cpp); src/verify/{primal_verifier,reference_lp_verifier,sparse_lp_verifier,linear_certificate}.cpp+headers (optional deadline params).
Contracts: (1) SolveOptions += `std::optional<double> total_time_limit_seconds` (>0,<=1e8) + `std::optional<std::size_t> memory_limit_bytes` (>0); SolveResult += `std::string stop_reason`. (2) Boundary builds ONE SolveContext (deadline=earliest, budget, thread_quota=num_threads, seed) passed run_engine→run_*/certify_mip (api_internal signature FIRST); finalize sets stop_reason; resource stop never accompanies unverified optimal/infeasible/unbounded (guard test). (3) StageScope per stage: lp canonicalize/presolve/scaling/dense/solve/postsolve/verify; qp model_build/solve/verify; pdlp solve/verify; milp/parallel search/verify/certify; nonlinear model_build/solve/verify; proof build/replay. (4) charge_or_stop at: working_model copy, parallel root_model, dense buffers, proof-build reserve → refusal = resource_limit + failure_site `memory_budget`; must not fall into generic numerical_failure handler. (5) verifiers get optional deadline checked between iterations; document bounded overrun (indivisible kernels not preemptible). (6) WorkerContext per parallel worker: charge sizeof(BranchNode) per push, note_local_stop→queue.request_stop, thread_quota enforced. (7) CLI `--memory-limit-bytes`; `--time-limit` ALSO sets total_time_limit_seconds (keep existing mappings); usage+validation; Python properties+kwargs+tests. (8) bad_alloc stays resource_limit/allocation_failure; nothing escapes entry points (extend injection sweep to hit LP/MILP/parallel/PDLP/QP engines, ≥1 mapped each, never infeasible/unbounded on failure).
Tests: expired total deadline → resource_limit+stop_reason; tiny memory_limit → resource_limit+memory_budget site; worker-kill demo script (spawn CLI, SIGKILL mid-solve, parent observes death, no false result, rerun completes; checkpointing = follow-up). Envelope table in evidence/resource-envelope-20260928.json + proposed STATUS lines; stages without preemption listed; IR-20/21 open; charge-points-only metering honest; RSS anomaly bounded only at charge points (ref evidence/ir19-w01-memory-20260928.json). Finish: full ctest build_item5, source-limits, wheel+pytest green.

## 6. baseline-freeze-agent brief (item 7)
Owned: scripts/support/run_full_*.{py}, scripts/run_{compare,netlib,miplib}.py, scripts/datasets.py, scripts/ml/ml_splits.py (freeze hook), NEW manifest modules; evidence/benchmarks/** + evidence/comparison/** NEW dirs only (never modify dated CSVs); evidence/baseline-freeze-20260928.json.
Deliverables: (1) frozen comparator manifest evidence/frozen-comparators-20260928.json (versions via probe at annotate.py:16-70, availability, probe commands). (2) frozen instance manifest + deterministic 70/15/15 family splits (hash approach from ml_splits.py) covering CURATED (compare_config.py:25-54) + SUITES (benchmark_config.py:11-45); missing optional datasets listed unavailable, no downloads. (3) timing constants as code (LP/QP 60s, MIP 300s, release 3600s, ≥5 repeats, process-wall vs engine-only, cold vs repeat) imported by both configs, documenting kept-vs-roadmap defaults. (4) regenerate baseline: `python3 scripts/run_full_compare.py --solver ./build_contracts/markov-cero-or-markov_cero_solve --out evidence/comparison/baseline_frozen_20260928` + `run_full_benchmark.py --suites netlib --out-dir evidence/benchmarks/baseline_frozen_20260928 --expected-solver-sha256 <sha>` — time-box ~20min, record ran/skipped+why, these supersede quarantined rows. Checks: source-limits; ctest -R "compare_harness|netlib_benchmarks|miplib_benchmarks|repository_tools" green.

## 7. repeated-solve-agent brief (item 8)
Owned: src/lp/**, include lp/**, src/qp/**, include qp/**, tests/{warm_start_property,dual_simplex,qp}_test.cpp, NEW tests, NEW scripts/bench_repeated_solve.py. NO src/api, apps, python, cmake, milp, refinery.
Scope: (1) measure first — bench harness over RHS-only/bounds-only repeat sequences: cold vs repeat wall/iterations/refactorizations (dual_simplex.hpp:56) + KKT refactor counts. (2) dual-simplex session reuse: keep BasisState+factorization across solves, cold fallback on degeneracy; expose header API (e.g. lp::dual::make_session) — engine wiring reported, not edited. (3) QP factor cache: shape/fingerprint-gated reuse in kkt_factor/admm_solver (today factorizes per solve :66, rho-only refactor :211-234), correct invalidation; cache miss ≡ current behavior. (4) verification on EVERY accepted result (verify_reference_result / verify_qp_solution per warm_start_property_test pattern); tests assert reuse-vs-cold parity AND verified each step. (5) keep only benchmark-justified optimizations; record rejects.
Report: tests+registration lines, before/after numbers, engine-hook follow-up lines, CHANGELOG/STATUS lines, evidence/repeated-solve-20260928.json.

## 8. refinery-schema-agent brief (item 10, code portion; IR-34 gates stay OPEN)
Owned: src/refinery/**, include refinery/**, tests/refinery_test.cpp+NEW, scripts/generators/gen_{refinery_scheduling,crude_blending,process_network,supply_chain,public_refinery}.py, data/refinery/fawley_public.json (tests updated if edited), refinery-specific apps (markov_cero_iis.cpp + refinery* if named-report flag added). refinery_model.cpp at 250 lines → NEW files for new logic.
Scope: (1) units schema (bbl/kbpd/d, t/kt, wt%↔ppm via density, RON/cetane, RVP psi, USD/bbl) with conversion helpers + mismatch rejection applied to every input/variable/constraint/objective/result label (plan :66-76). (2) quality schema: implement sulfur wt%↔ppm; keep honest rejection for unmodelled specs (preserve IR-14); unit-checked feed-balance assertions (IR-13 extension). (3) named reports: named rows/constraints, duals/slacks with units, active bound names, quality margins (slack-to-limit in unit), IIS row-vs-bound scope; JSON schema in evidence; CLI flag only on refinery entry. (4) generators emit units, deterministic. (5) evidence states engineer approval + shadow trial NOT obtained (IR-34/G8 open).
Report: tests+registration, report schema, CHANGELOG/STATUS lines with honest gate wording, evidence/refinery-units-schema-20260928.json. Verify: ctest -R "refinery|cli_refinery|cli_public_fawley|domain_" + source-limits.

## 9. packaging-release-agent brief (item 11, code portion; IR-33 gates stay OPEN)
Owned: setup.py, pyproject.toml, MANIFEST.in, cmake/Install.cmake, cmake/markov_ceroConfig.cmake.in, scripts/verify-release.sh, BUILDING.md, docs/governance/support-rollback.md (NEW), PROVENANCE.md (append-only release-qualification section if needed — otherwise report), evidence/packaging-qualification-20260928.json. Avoid cmake/CoreTargets.cmake (dirty, owned by main).
Scope: (1) packaging qualification: offline wheel install into fresh venv + pytest; `cmake --install` prefix + external consumer build/run (pattern: evidence/readiness-validation/installed-consumer.log); record matrix (gcc Release, python 3.14). (2) support+rollback doc: install/uninstall/upgrade/rollback drill with exact commands (validated by actually performing a rollback drill: install vN wheel, "upgrade" to rebuild, rollback to prior wheel, re-verify tests); support contact = placeholder honest (no SLA, no maintainer names we don't have — D18). (3) release verification script extension already covers gcc/clang — verify it runs; SBOM/signing = explicitly out of scope (IR-33 open). (4) release manifest: wheel sha + source manifest references.
Report: drill results, exact CHANGELOG/STATUS lines, gates-open wording (IR-33: SBOM/vuln review, clean commit, hosted evidence, support ownership), evidence path. Checks: sovereignty_guard + build_info + repository_tools ctests, source-limits.

## 10. proof-guarantee-agent brief (item 6; runs in two phases across waves)
p1 (Wave 1) Owned: include/markov_cero/verify/mip_proof.hpp, src/verify/mip_proof*.cpp, tests/mip_proof_test.cpp, tests/repository_tools_test.py, evidence/proof-guarantee-20260928.json. NOT src/api/solve.hpp/apps/python (owned by agent 5 concurrently).
p1 scope: (1) MipProof/MipProofReport metadata: assurance tier enum (independent_tree/replayed_tree/unverified), budget-consumed fields (nodes_used, witness_values_used, budget_time_ms, exhausted flag), proof format version, model fingerprint binding field; build/replay already timed (mip_certificate.cpp:19-44) — report carries its own timings so JSON export can follow. (2) exhausted-vs-rejected-vs-unsupported distinction in report status/message (consumption wiring to SolveResult happens in p2). (3) tests: metadata assertions, budget-exhausted report path, fingerprint binding; keep repository_tools assertions green (update expected JSON fields there — that file is yours). (4) large-case timing: run existing domain proof case (CTest domain_production_planning_large or direct CLI on its fixture) with build/verify timing recorded; extend to at least one larger certificate case than tests/mip_proof_test.cpp's 3-node proofs; record in evidence.
p2 (Wave 2, after agent 5 done): transfer ownership of solve.hpp, src/api/mip_certificate.cpp+engine_milp/parallel certify sites, apps/json_output.hpp, apps/markov_cero_solve.cpp, python/src/results.cpp for: expose tier/budget/exhausted in SolveResult (guarantee fields), JSON+CLI+Python surfaces, exhausted proof budget → certificate_type/status honestly (never claimed verified), tests in api_test/repository_tools + python.
Report each phase: files, tests+registration, CHANGELOG/STATUS lines, evidence.

## 11. milp-strengthen-agent brief (item 9; Wave 2, after agents 5 and 6a)
Owned: src/milp/** (search_separate_cuts, search_finish, search_context.hpp, search_node_relaxation/search_branch, cut_pool, gomory, mir, cover, heuristics*, node_lp, shared_incumbent, parallel_tree_search_root_cuts + worker-loop edits compatible with agent 5's WorkerContext), include/milp/** (cut_pool, cuts, node_view, milp_solver), tests/milp*_test.cpp+strong_branching+parallel tests, scripts/measure_cut_effectiveness.py, docs/codebase/technical-debt/root-only-cuts.md (fix stale paths), evidence/milp-strengthening-20260928.json. Proof obligations: extend src/verify/mip_proof* (freed after 6a) to record cut/propagation events with a cut-free replay-compatible proof-note (design: obligations recorded as annotations; replay stays cut-free unless invariant provable — honest choice documented).
Scope: (1) propagation producer: emit BoundEvidenceSource::propagated in node relaxation (currently enum with no producer) with tests. (2) parallel engine in-tree cuts (today root-only, parallel_tree_search_root_cuts.cpp catch(...) swallows — fix silent catch). (3) measured LP-bound/incumbent/search bottleneck split (Result counters/trace events) feeding evidence. (4) cut/propagation proof obligations per roadmap :430/:416 (fresh-reviewer language = report, external gate). (5) keep serial parallel parity: reuse search_separate_cuts frequency gating + revert-on-unverified (search_separate_cuts.cpp:97 pattern).
Report: tests+registration (cut_effectiveness CTest numbers), obligation design note, CHANGELOG/STATUS lines, evidence. Checks: ctest -R "milp|parallel|strong_branching|cut|mip_proof" + source-limits.

## 12. evidence-docs-agent brief (item 12 + integration; Wave 3, run by main agent with this prompt)
After all agents report: apply queued cmake/CHANGELOG/STATUS/register lines; write §14 notes for items 5-11 into the roadmap (pattern: existing "Implementation in progress" paragraphs); STATUS capability rows updated honestly (implemented/verified/gates-open); CHANGELOG entries per item; evidence index (evidence/backlog-5-12-20260928.json linking all new evidence files); promotion section: ONLY features with passing gates promoted; limitations published (IR-20/21 open with envelope, IR-33/34 open external, IR-28/35 unchanged, RSS anomaly unresolved); run FULL verification matrix; final report to user.

## 13. Verification matrix (main agent, end of Wave 3)
1. cmake -S . -B build_contracts (Release, WERROR) + full ctest — target all green.
2. build_asan_contracts ASan/UBSan ctest; build_tsan_contracts TSan ctest.
3. Wheel: `.venv/bin/python -m pip wheel . -w /tmp/opencode/wheels-final --no-deps --no-build-isolation` + `.venv/bin/python -m pytest python/tests -q`.
4. python3 scripts/check_source_limits.py; check_docs.py; check_json.py (+ manual json.load on all new evidence files).
5. Sanity: git status shows only intended files; register CSV parses; roadmap links valid.

## 14. Known risks / mitigations
- Agent 5 is oversized → it is the critical path; if it reports late, Wave 2 waits (nothing else blocks).
- Concurrent builds of different dirs are fine (parallel 4 each); CPU contention — stagger heavy builds if the machine has <16 cores.
- If a file conflict occurs despite ownership (e.g. engine signature change needed by 6b), integrator resolves manually; agents must report follow-ups rather than reaching outside ownership.
- 300-line limit will bite engine_lp.cpp (259) and api.cpp (227) when adding StageScope → agent 5 extracts stage helpers into new files (src/api/engine_stages.hpp/cpp).
- Tests count will grow from 89; each agent reports its local count; integrator reconciles.
