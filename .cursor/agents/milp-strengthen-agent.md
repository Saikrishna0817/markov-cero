---
name: milp-strengthen-agent
description: Implements roadmap backlog item 9 — propagation producer, in-tree cuts for the parallel engine, measured LP-bound/incumbent/search bottleneck split, and cut/propagation proof obligations. Wave 2 only (after items 5 and 6a release src/milp and src/verify). Use proactively for MILP cut, propagation, bottleneck or proof-obligation work.
---

# MILP Strengthen Agent (backlog item 9, Wave 2)

Execute item 9 of docs/audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md §14: "Strengthen MILP using measured LP-bound/incumbent/search bottlenecks; require cut/propagation proof obligations." START ONLY when the integrator confirms: item-5 agent finished (src/milp parallel slice free) AND item-6a agent finished (src/verify/mip_proof free). Verify both before editing.

## Conventions (mandatory)
- Repo: /home/saikrishna/markov-initial-build — uncommitted worktree. NEVER run git add/commit/push/stash/checkout/reset.
- Edit ONLY your owned files. NEVER edit: cmake/*, CHANGELOG.md, STATUS.md, VERIFY.md, evidence/defect-closure-register.csv, docs/audit/*.md, src/api/, apps/, python/, src/lp/, src/qp/, src/refinery/, scripts/support/* — report exact lines instead.
- 300-line hard limit per .cpp/.hpp/.py (`python3 scripts/check_source_limits.py` — run it; extract helpers).
- Tests: plain main() + require(bool, const char*) (copy tests/milp_cuts_test.cpp pattern). Report new test names; never edit cmake.
- Build dir: ONLY build_item9 (`cmake -S . -B build_item9 -DCMAKE_BUILD_TYPE=Release -DMARKOV_CERO_WARNINGS_AS_ERRORS=ON && cmake --build build_item9 --parallel 4`, `ctest --test-dir build_item9 -j4`).
- Style: mimic neighbors, no drive-by refactors. Respect the uncommitted item-4/5 state: read files BEFORE editing (parallel driver, search_context, work_queue were recently changed).
- Evidence: evidence/milp-strengthening-20260928.json (measurements, obligation design, limitations).

## Owned files
src/milp/** (search_separate_cuts.cpp, search_finish.cpp, search_context.hpp, search_node_relaxation.cpp, search_branch.cpp, cut_pool.cpp, gomory.cpp, mir.cpp, cover.cpp, heuristics*.cpp, node_lp.cpp, shared_incumbent.cpp, parallel_tree_search_root_cuts.cpp, parallel_tree_search*.cpp [coordinate with the item-5 WorkerContext slice already in place]), include/markov_cero/milp/** (cut_pool, cuts, node_view, milp_solver — node_bounds/work_queue careful), src/verify/mip_proof*.cpp + include/markov_cero/verify/mip_proof.hpp (only the obligation/annotation additions, preserving phase-1 metadata), tests/milp*_test.cpp, strong_branching, parallel tests, scripts/measure_cut_effectiveness.py, docs/codebase/technical-debt/root-only-cuts.md, evidence/milp-strengthening-20260928.json.

## Scope
1. **Propagation producer**: `BoundEvidenceSource::propagated` (node_view.hpp:23-50) has NO producer — emit it from node bound propagation (tightening bounds via constraints at node relaxation/branch) with tests proving: correct source label, affects materialization evidence, no behavioral regression (status/objective parity on existing MILP tests).
2. **Parallel in-tree cuts**: parallel engine cuts are root-only (parallel_tree_search_root_cuts.cpp:4-45) and `catch(...)` silently swallows failures — remove the silent swallow (record a counter/message), and reuse the serial freq-gated separation (search_separate_cuts.cpp:3-130 incl. revert-on-unverified at :97) inside parallel node processing safely (thread-safety: per-worker cut lists or queue-protected; no data race — TSan must pass).
3. **Measured bottleneck split**: add counters/events separating LP-bound time, incumbent/heuristic time, and search/bookkeeping time in `milp::Result` (extend alongside existing counters; report new Result fields for the integrator since milp_solver.hpp may be shared) — then MEASURE on a few instances (e.g. flugpl, stein15, pk1 via existing bench binaries or a small script) and record in evidence JSON. Use measurements to justify (or honestly not make) one targeted improvement — profile-driven only.
4. **Cut/propagation proof obligations** (roadmap :430 proof event interfaces; :416 fresh-reviewer language = external): design a minimal obligation record — each applied cut/propagation emits a verifiable obligation (cut coefficients/bound source + violated-lhs check or falsifiable certificate) attached to the proof artifact as annotations, WITHOUT breaking cut-free replay: replay path must remain valid when annotations are stripped (default) — i.e. obligations are AUDIT annotations, documented honestly as such unless you can make them replay-verified. Update mip_proof format versioning if the artifact changes; reader tolerance for missing annotations.
5. **Update docs/codebase/technical-debt/root-only-cuts.md** — fix stale paths (cites milp_solver.cpp:341-443; file is now 52 lines) and reflect parallel state after your change.
6. **Never claim**: broad performance gains (IR-35) — evidence is measurements on named instances only.

## Tests + report
Extend milp_cuts, new propagation-evidence test, parallel-cuts thread-safety (TSan run!), obligation encode/decode test; full ctest in build_item9 green + `ctest -R "milp|parallel|strong_branching|cut|mip_proof"`; source-limits clean. ALSO run TSan on your build (reconfigure with -DMARKOV_CERO_ENABLE_TSAN=ON in build_item9_tsan) for the parallel cut path. Final report: files, tests + registration lines, measurement table, obligation design note, exact CHANGELOG/STATUS lines, evidence path.
