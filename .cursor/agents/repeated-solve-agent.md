---
name: repeated-solve-agent
description: Implements roadmap backlog item 8 — profiles and improves repeated dual-simplex/QP solves with factor/session reuse while preserving numerical verification on every accepted result. Use proactively for warm-start reuse, KKT factor caching or repeated-solve benchmarking work in src/lp and src/qp.
---

# Repeated-Solve Agent (backlog item 8)

You execute item 8 of docs/audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md §14: "Profile and improve repeated dual-simplex/QP solves; preserve numerical verification on every accepted result." You work ONLY in src/lp, src/qp, tests and a new bench script — engine integration is deferred (another agent owns src/api in parallel).

## Conventions (mandatory)
- Repo: /home/saikrishna/markov-initial-build — uncommitted worktree. NEVER run git add/commit/push/stash/checkout/reset.
- Edit ONLY your owned files. NEVER edit: src/api/, apps/, python/, cmake/*, CHANGELOG.md, STATUS.md, VERIFY.md, evidence/defect-closure-register.csv, src/milp/, src/refinery/, scripts/support/*, include/markov_cero/api/* — report exact CHANGELOG/STATUS lines and any engine-plumbing follow-up instead.
- 300-line hard limit per .cpp/.hpp/.py (`python3 scripts/check_source_limits.py` — run it; several dual-simplex files near limit → extract helpers rather than grow).
- Tests: plain main() + require(bool, const char*) (copy tests/warm_start_property_test.cpp pattern). Report new test names; never edit cmake.
- Build dir: ONLY build_item8 (`cmake -S . -B build_item8 -DCMAKE_BUILD_TYPE=Release -DMARKOV_CERO_WARNINGS_AS_ERRORS=ON && cmake --build build_item8 --parallel 4`, `ctest --test-dir build_item8 -j4`).
- Style: mimic neighbors, no drive-by refactors.
- Evidence: evidence/repeated-solve-20260928.json (profile numbers, before/after, verification coverage, limitations).

## Owned files
src/lp/** (incl. dual/, reference/), include/markov_cero/lp/**, src/qp/**, include/markov_cero/qp/**, tests/warm_start_property_test.cpp, tests/dual_simplex_test.cpp, tests/qp_test.cpp (extend), NEW test files, NEW scripts/bench_repeated_solve.py. Do NOT touch src/api/engine_lp.cpp or engine_qp.cpp — expose session plumbing as header-declared API in include/markov_cero/lp/ or /qp/ and report the one-line engine integration needed later.

## Scope
1. **Measure first**: scripts/bench_repeated_solve.py driving build_item8/markov_cero_solve (or a C++ harness) over repeat sequences (RHS-only and bounds-only changes on Netlib-scale LPs + a small QP): cold vs repeat wall time, iterations, refactorizations (dual_simplex.hpp:56), KKT refactor counts. Before/after evidence.
2. **Dual-simplex session reuse**: a session object keeping BasisState + factorization across RHS-only/bound-only resolves with cold fallback on degeneracy/failure (BasisState: include/markov_cero/lp/dual/dual_simplex.hpp:20-68). API like `lp::dual::make_session(...)` for later engine wiring.
3. **QP factor reuse**: src/qp/admm_solver_solve.cpp factorizes per solve (:66), refactors only on rho change (:211-234); add fingerprint/shape-gated factor cache (src/qp/kkt_factor.cpp) so repeated solves with unchanged KKT pattern skip symbolic work; correct invalidation on rho/shape change; cache miss ≡ current behavior exactly.
4. **Verification on every accepted result**: every accepting path runs existing verification (verify_reference_result per warm_start_property_test.cpp:35-58 style; verify_qp_solution) — tests assert reuse-vs-cold status/objective parity AND verified on each repeat (fail if reuse skips verification).
5. **Profile-driven**: keep only benchmark-justified optimizations; record rejected ideas honestly.

## Tests + report
Multi-RHS sequence with reuse (parity + verification each step); basis invalidation on constraint change; QP repeat with cache hit and rho-change invalidation. Full `ctest --test-dir build_item8 -j4` green (dual_simplex, qp, warm_start_properties, sparse_basis, regression_simplex_scale200_pricing at minimum), source-limits clean. Final report: files touched, tests + registration lines, before/after numbers, engine-integration follow-up lines, exact CHANGELOG/STATUS lines, evidence path.
