---
name: proof-guarantee-agent
description: Implements roadmap backlog item 6 in two phases — proof metadata/assurance tiers and budget-exhaustion semantics (phase 1: src/verify), then richer guarantee fields surfaced through SolveResult, JSON, CLI and Python (phase 2: src/api after item 5 lands). Use proactively for mip_proof metadata, certificate tiers or guarantee-field work.
---

# Proof Guarantee Agent (backlog item 6, two phases)

Execute item 6 of docs/audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md §14: "Configurable proof budgets and separate build/replay timing are implemented; add richer guarantee fields and validate timing on larger certificate cases." Budgets (solve.hpp:36-39) and split build/replay timing (mip_certificate.cpp:19-44) EXIST — you add the missing guarantee/assurance/budget-visibility surface + large-case timing evidence.

## Conventions (mandatory)
- Repo: /home/saikrishna/markov-initial-build — uncommitted worktree. NEVER run git add/commit/push/stash/checkout/reset.
- Edit ONLY files owned by your CURRENT phase (below). NEVER edit: cmake/*, CHANGELOG.md, STATUS.md, VERIFY.md, evidence/defect-closure-register.csv, docs/audit/*.md — report exact lines instead.
- 300-line hard limit per .cpp/.hpp/.py (`python3 scripts/check_source_limits.py` — run it).
- Tests: plain main() + require(bool, const char*) (copy tests/mip_proof_test.cpp / tests/api_test.cpp pattern). Report new test names; never edit cmake.
- Build dirs: phase 1 → build_item6; phase 2 → build_item6 (reuse). Release + -DMARKOV_CERO_WARNINGS_AS_ERRORS=ON. Never touch other build_* dirs.
- Evidence (own file): evidence/proof-guarantee-20260928.json (one file updated across both phases; include large-case timings + limitations).

## Phase 1 (Wave 1, runs NOW) — OWNERSHIP
Owned: include/markov_cero/verify/mip_proof.hpp, src/verify/mip_proof.cpp, src/verify/mip_proof_builder.cpp, src/verify/mip_proof_io.cpp, src/verify/mip_proof_relaxation.cpp (+ verify headers), tests/mip_proof_test.cpp, tests/repository_tools_test.py, evidence/proof-guarantee-20260928.json.
FORBIDDEN in phase 1 (another agent owns them concurrently): src/api/**, include/markov_cero/api/**, apps/**, python/**.

Phase 1 scope:
1. **Proof metadata**: extend `MipProof`/`MipProofReport` (mip_proof.hpp:16-35) with: assurance tier enum (`independent_tree` / `replayed_tree` / `unverified`), budget-consumed fields (nodes_used, witness_values_used, budget_time_ms, exhausted flag), proof format version, model fingerprint binding field (std::string, filled at build from an argument; empty when unbound — wiring to SolveResult::model_fingerprint happens in phase 2).
2. **Exhaustion semantics**: distinguish exhausted-vs-rejected-vs-unsupported in report status/message (today mip_certificate.cpp:39-45 only sets proof_message; the report itself must say). Builder already stops on deadline (mip_proof_builder.cpp:13) / witness cap (:20) / node cap (:38) — surface which budget hit.
3. **Timing**: build/replay timings already on SolveResult — also carry them into the report struct so the JSON export (phase 2) is trivial.
4. **Tests**: metadata assertions; budget-exhausted report path (tiny maximum_nodes → exhausted=true, tier=unverified, message names the budget); fingerprint binding present/absent; round-trip through mip_proof_io preserves new fields; update tests/repository_tools_test.py expectations for new JSON fields (keep exit-code contracts).
5. **Large-case timing**: run a larger certificate case than the 3-node tests — use the domain production-planning fixture (CTest domain_production_planning_large pattern: solve CLI with --mip-gap on its instance) and/or evidence/readiness-validation production harness instance; record build_ms/verify_ms/checked_nodes with budgets (e.g. 45s build/10s replay) in the evidence JSON. ≥2 cases if time allows.
6. **Serialization compatibility**: proof format version bump where structure changes; reader rejects unknown versions with clear error (mip_proof_io deadline/budget checks stay).

## Phase 2 (Wave 2, starts ONLY after the item-5 agent reports done — integrator will tell you)
Ownership transfers to: include/markov_cero/api/solve.hpp, src/api/mip_certificate.cpp, src/api/engine_milp.cpp, src/api/engine_parallel.cpp, apps/json_output.hpp, apps/markov_cero_solve.cpp, python/src/results.cpp, tests/api_test.cpp (coordinate: read current state first — the item-5 agent has already modified these), python/tests as needed.
Phase 2 scope: surface tier/budget/exhausted/fingerprint in `SolveResult` (guarantee fields: e.g. `std::string guarantee_tier`, `bool proof_budget_exhausted`, existing timings), JSON+CLI+Python surfaces; exhausted budget ⇒ certificate_type/status honestly reflect unverified (never `independent_mip_tree` claimed verified when exhausted — check engine_milp.cpp:33 / engine_parallel.cpp:32 set `incumbent_feasibility; solver_trusted_tree` fallback); tests in api_test + repository_tools + python.

## Checks + report (each phase)
ctest green in your build dir (mip_proof, repository_tools, api_test in phase 2; plus suite grows), source-limits clean; phase 2 adds wheel+pytest (`.venv/bin/python -m pip wheel . -w /tmp/opencode/wheels6 --no-deps --no-build-isolation && .venv/bin/python -m pytest python/tests -q`). Final report per phase: files, tests + registration lines, exact CHANGELOG/STATUS lines, evidence path, follow-ups.
