---
type: competitive-note
tags: [competitive, t2, sih26119]
status: verified
source: github.com/AryanMotiani/sovereign-solver
cloned: 2026-09-25
threat: medium
---

# sovereign-solver (AryanMotiani)

> T2. Full engine set in Python — evidence is 4 Netlib rows, one of them **wrong**. No CI,
> no licence.

## Engines [Observed]

- 60 Python files, 15.6k LOC: dense + revised simplex (`solver/lp/simplex_dense.py`,
  `simplex_revised.py`), **IPM** (`interior_point.py`), PDHG (`pdhg.py`), presolve package,
  B&B + branching + **Gomory/MIR/cover/clique cuts** + heuristics (`solver/milp/*`),
  ADMM QP (`solver/qp/admm.py`).
- **GPU not implemented** (README.md:243).

## Evidence (R16 genuine but tiny) [Observed]

- `benchmarks/results/netlib_benchmark.csv:2-5` — only 4 instances: afiro/adlittle match
  HiGHS ~1e-16; **`lseu` WRONG** (known 1120 vs theirs 834.68, `obj_match=False`); 4th row
  whatever it holds. A committed wrong-answer row is either brave bookkeeping or an unwatched
  harness — either way, thin.
- `benchmark_comparison.csv` (43 lines), `BENCHMARK_REPORT.md`.
- `highspy` used in `tests/test_revised_simplex.py:139-166` + `benchmarks/netlib_benchmark.py`;
  solver package imports only SuperLU (`solver/utils/sparse_lu.py:73`) → R10 clean in
  substance, but `highspy` sits in top-level `requirements.txt` (grey area, cf.
  [[VX03 (VioniX37)]]).

## Weak spots

- No CI (`.github/` absent), no LICENSE file, no GPU, wrong Netlib row committed without note.

**Respect:** breadth in Python (closest to our engine-set in shape). **Beat:** correctness
watch + evidence volume — 4 rows with 1 wrong is the weakest T2 evidence ratio.
Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]