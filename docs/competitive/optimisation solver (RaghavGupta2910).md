---
type: competitive-note
tags: [competitive, t2, sih26119]
status: verified
source: github.com/RaghavGupta2910/optimisation_solver
cloned: 2026-09-25
threat: medium
---

# optimisation_solver (RaghavGupta2910)

> **Rank 8 (T2 leader).** 25.8k LOC of clean MIT C++ with real tests and 22 MPS fixtures —
> but no CI, no committed results, no IPM, no GPU.

## Engines [Observed]

- Modules: `src/{solver,presolve,postsolve,mps,model,adapter,benchmark,util}` + sub-engines
  `pdlp_engine/` (first-order PDLP, `pdlp_solver.cpp`), `milp_engine/`, `qp_engine/`
  (ADMM `admm_solver.cpp`), B&B (`branch_and_bound.cpp`, `branching_rules.cpp`),
  presolve/postsolve, MPS reader with a 22-file fixture corpus (`01_basic_lp.mps` …
  `22_integral_bounds.mps` incl. QMATRIX, SOS, RANGES, objsense).
- Parallel: `src/util/parallel.cpp` (thread usage) — present, quality unmeasured.
- **No `.cu` files and no `*ipm*`/`*interior*` paths anywhere** → no GPU, no IPM (R4 gap).
- 25,840 LOC counted across .cpp/.h; tests under `tests/{presolve,pipeline,cli,adapter}` and
  `tools/reference_checks/`.

## Evidence (R16) — claim-in-comments only [Observed]

- **Zero committed results files** (`find -name '*.csv' -o -name '*results*'` → none outside
  `benchmark_model/`).
- HiGHS appears only in code comments as a convention cross-check: `src/adapter/pdlp_adapter.cpp`
  "Verified against HiGHS on both senses…", `include/adapter/pdlp_adapter.h:…` "same
  convention HiGHS reports" — i.e. they ran it locally, committed nothing.
- `submission/PRESENTATION.md`, `submission/DEMO.md` exist (pitch artifacts).

## Weak spots

- No CI (no `.github/`), no IPM, no GPU (despite PS title), no committed benchmark evidence.
- MIT `LICENSE` present — one of the T2 with clean licence hygiene.

## R10 check

No external solver linked or vendored (HiGHS references are comments only). Clean.

**Respect:** test discipline (adversarial presolve, postsolve integration, CLI tests) —
comparable to ours in structure. **Beat:** evidence — they have none committed; we at least
have Netlib/MIPLIB CSVs and 43 ctest targets. Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]