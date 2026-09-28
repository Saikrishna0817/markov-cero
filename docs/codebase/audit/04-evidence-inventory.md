---
type: codebase-evidence
tags: [codebase, evidence, audit, benchmarks]
status: verified
verified_on: 2026-09-25
evidence:
  - "evidence/"
  - "reports/"
  - "docs/sih26119_problem_statement.md:62-80"
---

# Evidence Inventory

> Every evidence/report artifact, what it measures, how many instances, whether hardware is recorded, and which PS requirement (R1–R20, `docs/sih26119_problem_statement.md:62-80`) it supports.

Hardware column is **No** for every row unless stated: `rg -i "cpu|gpu|nvidia|xeon|ryzen|rtx|processor"` over all evidence CSV/JSON returns only instance names (`evidence/benchmarks/phase4.json:5,18`).

| file | measures | instances | hw recorded? | supports |
|---|---|---|---|---|
| `evidence/netlib_results.csv` | simplex obj vs reference, rel err, phase I/II iters, `runtime_ms`, primal/dual violations | 7 Netlib | No | R15, R19, R9, R20 |
| `evidence/netlib_extended.csv` | same + `reproducibility=requires network` column | 5 Netlib (max 389 rows) | No | R15, R19, R12(partial) |
| `evidence/miplib_results.csv` | MILP nodes, LP iters, cuts, heuristics, `runtime_ms`, violations | 3 MIPLIB (max 18 rows) | No | R15, R19, R5 |
| `evidence/benchmarks/phase4.json` | simplex vs PDLP timing; cut node reduction; 1/2/4-thread scaling | 2 LP + 2 MIP + 1 scaling run | No | R7, R5, R4(first-order), R2 |
| `evidence/benchmarks/crossover_study.csv` | simplex / CPU PDLP / GPU PDLP timings + 4-part GPU telemetry per scale | 13 synthetic `SCALE_5`–`SCALE_10000` | No | R8, R12, R6 |
| `evidence/benchmarks/crossover_plot.svg` | visual of the same crossover data | 13 | No | R8 |
| `evidence/benchmarks/gpu_profile_summary.json` | H2D/kernel/D2H/total ms, GFLOPS, occupancy, kernel shares | 1 (`SCALE_1000`, 525×980) | No (kernel stats only) | R8, R6 |
| `evidence/benchmarks/nsight_profile_analysis.md` | roofline, residency proof, occupancy narrative | 1 (`SCALE_1000`) | No | R8, R6 |
| `evidence/local-verification-report.txt` | sovereignty check + 2 full `ctest` runs, **41/41 passed** (2026-09-21) | n/a (41 tests) | No (OS only) | R10, R1, R14 |
| `evidence/environment-local.json` | `os`, `machine`, `python` only | n/a | **partial** — no CPU/GPU | R18, R20 |
| `evidence/deployment/fuzz-mps`, `fuzz-sparse` | intended fuzz corpora — **both directories empty** | 0 | n/a | R13 (absent) |
| `reports/crossover_study.csv` *(gitignored)* | byte-identical copy of `evidence/benchmarks/crossover_study.csv` (md5 `8cfde0d6…`) | 13 | No | R8 |
| `reports/gpu_benchmark.csv` *(gitignored)* | GPU vs CPU vs simplex run — truncated to `AFIRO` only | 1 | No | R8 |
| `reports/crossover_plot.svg` *(gitignored)* | plot copy | 13 | No | R8 |
| `_deployment-phase2-build/gpu_benchmark.csv` *(gitignored)* | 3-way comparison incl. `BLEND … NumericalFailure` | 4 | No | R8, R9, R15 |

## Observed Facts
- No file in `evidence/` or `reports/` names HiGHS/CPLEX/Gurobi/CBC/SCIP (`rg -i "highs|cplex|gurobi|cbc|scip" evidence/ reports/` → zero matches) → **R16 unsupported**.
- No Mittelmann instance set and no QPLIB data (`data/qp/` is empty) → R15/R19 partial.
- Largest measured real instance is 389 rows; nothing ≥10k rows → R12 unproven on real data.

## Impact (Inference)
- R7/R8 evidence is actively counter-evidential (0.56× speedup, 0.0% cut reduction, 1 simplex failure) — see [[negative-parallel-scaling]], [[root-only-cuts]], [[blend-numerical-failure]].
- Missing hardware metadata makes every timing non-reproducible ([[missing-hardware-metadata-in-evidence]]).

## Related
- [[testing-gaps]] · [[no-external-baseline]] · [[benchmark-suites]]
