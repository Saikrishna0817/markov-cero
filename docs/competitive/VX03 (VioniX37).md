---
type: competitive-note
tags: [competitive, t1, sih26119]
status: verified
source: github.com/VioniX37/VX03
cloned: 2026-09-25
threat: high
---

# VX03 (VioniX37)

> **Rank 3.** Polished Python stack: HSD-IPM + PyTorch GPU + CI job that *runs* the HiGHS
> benchmark on every push. Soft spots: non-OSI licence and solver deps in `requirements.txt`.

## Engines [Observed]

- Revised simplex + basis LU (`sovereign_opt/solvers/lp/simplex_engine.py`, `basis.py`).
- **Homogeneous self-dual IPM + crossover** (`solvers/lp/interior_point.py`) — R4 covered.
- Halpern PDLP/PDHG with **PyTorch CUDA** (`pdlp.py`, `pdhg_kernels.py`).
- Branch-and-cut + GMI/cover cuts (`milp/branch_bound.py`, `cuts.py`); QP via IPM + active set;
  concurrent racing + ML strategy selector.
- 94 Python files, 14.6k LOC (`sovereign_opt` 9.9k, `benchmarks` 2.4k, `tests` 1.2k).

## Evidence (R16 — genuine, CI-gated) [Observed]

- `benchmarks/results/netlib_20260917-093903.md`: **23/23 "match (certified)" vs HiGHS**,
  timings honest — theirs 13–350× slower.
- README.md:150-161 table: 11/13, QP 16/17, **MIPLIB 2/11**, robustness 19/22 (weak legs
  published, not hidden).
- Oddity: `e226` row rel.err **3.8e-01** labelled "match" — a tolerance-accounting bug worth
  noting if we quote their numbers.
- CI `.github/workflows/ci.yml`: pytest on Python 3.10/3.11/3.12, then
  `benchmarks.compare --suite ci --time-limit 120` **vs highspy**, plus a Next.js build.

## The `Gurobi files/` question (R10)

10 standalone scripts (946 LOC) `import gurobipy` to produce reference objectives —
**benchmark use, not dependency**: `sovereign_opt/` has zero `highspy`/`gurobipy` imports,
enforced by `tests/test_benchmarks.py:109-116`. **But** `requirements.txt` still lists
`highspy>=1.7.0` and `gurobipy>=12.0` (duplicated as "comparison only" in
`requirements-bench.txt`), so CI installs them — R10 risk = medium (grey area: compare ≠
build upon, but a strict reading of "shall not be built upon any existing solver library"
makes visible solver installs uncomfortable).

## Weak spots

- **Licence: PolyForm Strict 1.0.0** — source-available, non-OSI, no redistribution: fatal
  for a *sovereign* public-good pitch (our Apache-2.0 beats it outright).
- Python-first performance (13–350× slower than HiGHS is their own number).
- MIPLIB 2/11 — MILP leg weak, same as several rivals.

**Respect:** CI-run comparison + certified-match language + self-critical tables.
**Beat:** licence + native performance + provenance. Related: [[19-competitive-landscape]] §19.7.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]
- [[sovereign-solver (AryanMotiani)|competitive/sovereign-solver (AryanMotiani)]]