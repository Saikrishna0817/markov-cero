---
type: codebase-tech-debt
tags: [codebase, technical-debt, parallelism, performance]
severity: high
status: verified
verified_on: 2026-09-25
evidence:
  - "evidence/benchmarks/phase4.json:56-62"
  - "docs/audit/00-ground-truth.md:190"
---

# Negative Parallel Scaling

> The repo's own phase-4 evidence shows 4-thread search is slower than single-threaded (0.56× speedup).

## Observed Facts
- `evidence/benchmarks/phase4.json:56-62` (instance `stein9.mps`): `t1_ms` 0.77844, `t2_ms` 1.172218, `t4_ms` 1.383641, `speedup_2th` "0.66x", `speedup_4th` "0.56x", `efficiency_4th` "14.1%", with `identical_optima` and `zero_trust_verified` both true.
- Timestamp of the run: `evidence/benchmarks/phase4.json:2` → "2026-09-19 13:18:55 UTC".
- Parallel path exists in `src/milp/parallel_tree_search.cpp` (C++20 `std::jthread` per `docs/history.md`); no test asserts speedup > 1 (`CMakeLists.txt:187` registers `parallel_tree_search` as a functional test only).
- Audit record agrees: `docs/audit/00-ground-truth.md:190` ("4-thread speedup 0.56× (i.e., slowdown) … R7 effectively unmet").

## Impact (Inference)
- Defaulting to multi-threaded search can make MILP solves slower; the feature as measured is a net regression on this instance.
- R7 (multi-core parallelization) is demonstrated as functional, not beneficial — the evidence contradicts a performance claim.

## Related
- [[root-only-cuts]] · [[missing-hardware-metadata-in-evidence]] · [[testing-gaps]]
