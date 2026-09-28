---
type: competitive-note
tags: [competitive, t1, sih26119, pypi]
status: verified
source: github.com/Lothnic/IGAOS, pypi.org/project/igaos
cloned: 2026-09-25
threat: medium-high
---

# IGAOS (Lothnic)

> **Rank 6 — the packaging threat.** Only competitor with a PyPI release and a deck→artifact
> traceability ledger; but its README/PyPI headline numbers **contradict its own committed
> CSVs**, and its engines are the weakest of T1.

## Engines

- Primal + dual revised simplex with warm starts (`src/simplex/simplex.cpp` 90 KB), GPU PDHG
  (`src/pdhg/pdhg.cu` 52 KB, CUDA RTX 4060 / Ryzen 7840HS), B&B with pseudocosts/RINS/diving,
  root Gomory only, ADMM QP, MPS reader with RANGES.
- **No IPM** (zero ipm/interior/crossover paths in git tree) and **no parallelism** (serial
  `while(true)` tree loop in `src/milp/milp.cpp`, no thread flags in CMake).
- **MILP is the weak engine**: root-only Gomory with a "≥5% bound closure" gate that drops all
  cuts otherwise; no MIP presolve (their own deck admits it); MIPLIB full = 4/240 closed,
  185 time-limits @60 s [Observed: `docs/research/miplib_full_results.csv`].
- **Simplex ~10–100× slower than HiGHS** on medium/large LPs [Observed: e.g. ken-18 60 s
  time-limit vs HiGHS 6.01 s in `netlib_full_results.csv`].

## Evidence & CI (R16)

- `docs/research/benchmark-protocol.md` pins **HiGHS v1.15.1** with verbatim flags
  (`--presolve=on --parallel=off --threads=1 --random_seed=0`), tiered time limits, shifted
  geometric mean, fair-comparison rules — **the best-written protocol of any rival**; but
  published CSVs carry no machine-sheet/git-Sha/HiGHS-version footer, hardware sheets live in
  gitignored `benchmarks/` (**[Not found]** in repo), and actual runs used 60 s caps vs the
  protocol's 300/1800 s.
- `docs/research/industry-comparison.md`: same-machine HiGHS 1.15.1 vs **Gurobi 13.0.3** vs
  IGAOS. `gpu_solver_comparison.md`: head-to-head vs cuPDLP-C/HiGHS PDLP/OR-Tools PDLP with
  self-critical notes (pre-fix: cuPDLP-C 12–27× faster than them).
- CI `.github/workflows/ci.yml`: build+`ctest`+binding parity+QP three-way + wheel smoke —
  no sanitizers, no GPU job, no benchmark regression.
- PyPI `igaos` 0.1.0 (2026-08-29, 5 ABIs + sdist); description = stale README.

## The exploitable seam: internal inconsistency [Observed]

| README.md:79-81 (also PyPI) | Their own artifacts |
|---|---|
| Netlib "52/64 exact" | `netlib_full_results.csv` 114 rows: **100 exact**, 12 MISMATCH, 1 n/a; deck says **63/64**; |
| MIPLIB "6/20 @1e-4" | `miplib_results.txt:22` **"8/20"**; deck §7 **10/20** |
| robustness "10/15" | `robustness_results.csv` **12 PASS / 3 FAIL**; deck 12/15 |

Also: robustness `boeing1` parsed `ranges_parsed=89` vs suite manifest `ranges: 45` yet PASS;
suite doc status "**DRAFT (awaiting human lock)**"; MIPLIB CSVs show only `ref_objective`
comparison (protocol §4.3 mandates official checker — [Claimed only]); 4 smoke test files only.

## Weak spots

No IPM, no parallel, weak MILP, no provenance/clean-room doc (`docs/SOURCES.md` is a source
list), no hardware metadata in results, headline staleness.

## R10 check

`docs/DEPENDENCIES.md:16` forbids OSQP/SCS/Highs/CBC/GLPK/SCIP/OR-Tools as linked/vendored;
`highspy==1.15.1` is CI/test-only (wheels `requires_dist: null`). Licence: MIT.

**Respect:** distribution + protocol discipline + honest failure records. **Beat:** their
actual CSVs (100/114) are a bar we can quote; never repeat their README's 52/64 as fact.
Related: [[19-competitive-landscape]] §19.6–19.8.

## Referenced By

- [[Competitive Landscape MOC|competitive/Competitive Landscape MOC]]
- [[Igaos-public (trijalpgunaseelan)|competitive/Igaos-public (trijalpgunaseelan)]]