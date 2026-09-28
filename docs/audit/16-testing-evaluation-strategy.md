---
type: audit
phase: 16
title: Testing & Evaluation Strategy
tags: [audit, testing, evaluation, ci, phase-16]
status: complete
date: 2026-09-25
---

# Phase 16 — Testing & Evaluation Strategy

Ground truth for what is tested today, what must be added so every requirement is *provable*
rather than *asserted*, and how CI keeps it provable. Sources: [[00-ground-truth]] §C,
[[09-research-code-alignment]] §6.6 (experiments E1–E6), [[10-sih-evaluator-report]] §7.4,
[[12-keep-remove-rebuild]] (RW-1…RW-10), [[13-restart-point]] (steps 0–8), and the vault notes
[[testing-gaps]] · [[benchmark-suites]] · [[04-evidence-inventory]].

## 16.1 Current test reality (Observed)

| Fact | Evidence |
|---|---|
| 43 `add_test` targets, all declared in the root `CMakeLists.txt:166-225` (no `tests/CMakeLists.txt`) | `grep -c add_test CMakeLists.txt` = 43 |
| 22 CPU test sources in `tests/*.cpp` + 8 GPU sources in `gpu/tests/*.cpp`; `tests/fuzz/` holds 4 more, only `mps_fuzz_smoke.cpp` is wired | `docs/codebase/tests/overview.md:18` |
| Suite shape: 31 unit/component, 1 `json_records` (JSON syntax only), 1 `sovereignty_guard`, 6 `cli_*`, 4 benchmark runners | `CMakeLists.txt:166-225` |
| Hand-rolled framework (`tests/support/tiny_exact.hpp`); gtest/gmock forbidden by `scripts/check-sovereignty.py:23` | [[testing-gaps]] |
| CI = 8 jobs (gcc/clang × Debug/Release/ASan-UBSan/TSan); **no CUDA job**, `MARKOV_CERO_ENABLE_CUDA` defaults OFF (`CMakeLists.txt:7`) | `.github/workflows/ci.yml:16-44` |
| Last *recorded* run evidences 41/41 tests, not 43 (`qp`, `cli_qp_portfolio` added later) | `evidence/local-verification-report.txt:236` |
| Evidence files: `netlib_results.csv`, `netlib_extended.csv`, `miplib_results.csv`, `benchmarks/{phase4.json,crossover_study.csv,gpu_profile_summary.json}` | [[04-evidence-inventory]] |
| **Known bug:** `scripts/run_gpu.py:241-245` gates `inst_passed` on GPU status/verification and CPU-PDLP status but never on `res_simplex["status"]` → `BLEND → NumericalFailure` still exits 0 | `_deployment-phase2-build/gpu_benchmark.csv:3` |
| No baseline, no repeats, no Mittelmann/QPLIB data, largest real instance in evidence = 389 rows | [[testing-gaps]], [[benchmark-suites]] |

## 16.2 Strategy per layer

| Layer | What it must show | Harness / command | R# |
|---|---|---|---|
| **Unit** | LP/QP/MILP kernels are correct *and independently certified* — residuals, not status strings | `ctest -R "primal_simplex\|dual_simplex\|sparse_basis\|qp"`; verifiers assert primal/dual/KKT residuals ≤ tol | R2, R6, R9 |
| **Integration** | CLI end-to-end: parse → solve → verify → JSON, incl. failure paths | `ctest -R "cli_"` (`WILL_FAIL` cases for infeasible/malformed/limit) | R14 |
| **Benchmark** | Results on the four named suites | `netlib_benchmarks`, `miplib_benchmarks`, new `mittelmann_benchmarks`, new `qplib_benchmarks` | R15, R19 |
| **Robustness** | Degenerate / ill-conditioned / weak-relaxation instances converge without `NumericalFailure` = **experiment E3** | new `robustness_suite` ctest + `evidence/robustness-dossier.md` | R13, R17 |
| **Regression** | Nothing that worked yesterday breaks today | golden results: fixed instance set → objective/status/KKT must match `tests/golden/*.json` | R9, R18 |
| **GPU** | GPU result ≡ CPU result (status + objective within tol), and every status is checked | `equivalence`, `gpu_pdhg_*`, plus new `gpu_status_gate` that fails on any non-Optimal row | R8 |
| **Parallel** | Scaling curve 1/2/4/8, reported as efficiency not as a single speedup | new `parallel_scaling` ctest → `evidence/benchmarks/parallel_scaling.csv` | R7 |
| **Cross-solver** | Head-to-head on a shared instance set = **experiment E1** | new `compare_harness` ctest driving `scripts/run_compare.py --baseline highs` | R16, R20 |

## 16.3 Concrete test additions (≥10)

| # | Test file / executable (ctest name) | Asserts | R# / experiment |
|---|---|---|---|
| 1 | `tests/compare_harness_test.py` (**`compare_harness`**) | `run_compare.py` emits one row per shared instance with markov-cero *and* HiGHS status/objective/time; relative objective disagreement ≤ 1e-6 on ≥ 9/10 Netlib instances; table + profile files exist | **R16**, R20, E1 |
| 2 | `tests/robustness_suite_test.cpp` (**`robustness_suite`**) | For each curated degenerate / ill-conditioned / weak-relaxation MPS: terminal status is Optimal-or-feasible (never `NumericalFailure`) and independent KKT residual ≤ 1e-6 | **R13, R17**, E3 |
| 3 | `tests/golden_results_test.cpp` (**`golden_results`**) | 12-instance golden set matches `tests/golden/*.json` on status, objective (rel 1e-9), and certificate verdict — catches silent numerical drift | R9, R18 |
| 4 | `tests/cut_effectiveness_test.cpp` (**`cut_effectiveness`**) | nodes(`--cuts`) ≤ 0.8 × nodes(`--no-cuts`) on ≥ 1 MIPLIB instance (≥ 20 % cut-node reduction) | **R5**, E6, RW-1 |
| 5 | `tests/parallel_scaling_test.cpp` (**`parallel_scaling`**) | S₄ ≥ 1.0 and E₄ ≥ 0.5 on the fixed scaling instance; if not met the parallel engine must be reported unsupported, not claimed | **R7**, E5, RW-2 |
| 6 | `tests/ipm_crossover_test.cpp` (**`ipm_crossover`**) | New IPM + crossover reproduces the simplex objective within 1e-8 on 5 LPs and returns a valid basis | **R4** |
| 7 | `tests/gpu_status_gate_test.py` (**`gpu_status_gate`**) | Re-runs `scripts/run_gpu.py --instances blend`; exit code is non-zero whenever any `simplex_status` ≠ `Optimal` (regression guard for `scripts/run_gpu.py:241-245`) | **R8**, RW-9 |
| 8 | `tests/scale_path_test.cpp` (**`scale_path`**) | ≥ 10k-row sparse MPS solves through the sparse-first path with no dense-gate fallback and status Optimal | R12, RW-5 |
| 9 | `tests/presolve_rules_test.cpp` (extend `presolve`) | Each new rule (implied bound, forcing/dominated row, duplicate row) fires on a crafted model and postsolve round-trips to the original objective exactly | R5, R9, RW-6 |
| 10 | `tests/iterative_refinement_test.cpp` (**`iterative_refinement`**) | Residual drops ≥ 1e2 after refinement on a κ ≈ 1e10 system; without it the solve is rejected | R9, R17, RW-8 |
| 11 | `tests/json_schema_test.py` (**`json_schema`**) | Solution JSON validated against `spec/solution.schema.json`: `status`, `verified`, `canonical_verified`, `original_verified`, objective, gap — replaces syntax-only `json_records` (`CMakeLists.txt:197`) | R14, R18 |
| 12 | `tests/certificate_cli_test.cpp` (**`cli_certificate`**) | `markov-cero-solve examples/qp_portfolio.mps --engine qp --output x.json` produces `verified:true` with both gates true, and the console prints `QP KKT certificate verified` | R14, R17 |
| 13 | `tests/mittelmann_smoke_test.py` (**`mittelmann_benchmarks`**) | Mittelmann LP subset status/objective match reference within 1e-6; skips cleanly (not silently) when instances absent | R15, R19 |

## 16.4 Metric definitions (each belongs to an existing note)

| Metric | Formula (brief) | Note |
|---|---|---|
| Relative optimality gap | `g = (UB − LB) / max(\|UB\|, ε)` — report UB *and* LB; a limit-truncated gap is not "optimal" | [[Relative Optimality Gap]] |
| Geometric mean runtime | `GM = exp((1/n)·Σ ln t_i)`; for solver ratios `r_i = t_A/t_B`, GM of ratios answers "how many times slower"; timeouts get a fixed penalty time before the log | [[Geometric Mean Runtime]] |
| Dolan–Moré profile | `r_{s,p} = t_{s,p} / min_{s'} t_{s',p}`; `ρ_s(τ) = (1/n)·\|{p : r_{s,p} ≤ τ}\|` — the distribution behind the GM scalar; declare the penalty for unsolved instances | [[Geometric Mean Runtime]] (companion), [[Dolan-2002-Benchmarking-Optimization-Software]] |
| Cut-node reduction | `1 − N_nodes(cuts on) / N_nodes(cuts off)` — *kept* cuts and the bound/node change they bought, not generation volume | [[Cut Efficiency]] |
| Parallel efficiency | `E_p = S_p/p = T_1/(p·T_p)`; 0.56× at p=4 is legible only as E = 14 % | [[Parallel Efficiency]], [[Parallel Speedup]] |
| KKT residual | `max` of stationarity / primal / dual / complementarity violations, each normalized by a data scale; checkable arithmetic independent of the solver's status string | [[KKT Residual]], [[Primal Residual]] |

**Rule:** no number appears in README, STATUS, the deck or the video unless it is produced by
one of the six metrics above *and* names the artifact and hardware that produced it.

## 16.5 CI changes needed

1. **Comparison job (`compare`, ubuntu-latest):** install HiGHS (pip/apt, external — `comparing ≠
   building upon`, so `sovereignty_guard` is untouched), run `ctest -R compare_harness`, upload
   `evidence/comparison/{table.csv,profile.svg,geom_means.md}` as an artifact; fail the job on
   objective disagreement beyond tolerance.
2. **GPU job — choose one, explicitly:** (a) a CUDA-capable runner configuring
   `-DMARKOV_CERO_ENABLE_CUDA=ON`, running the 8 GPU tests + `gpu_status_gate` + `gpu_benchmarks`;
   or (b) **claim demotion**: README/STATUS state "GPU code ships unexercised by CI" and the GPU
   headline is removed until (a) exists (RW-9). Silence is not an option (see [[10-sih-evaluator-report]] K2).
3. **Hardware manifest:** every job writes `evidence/hardware.md` (`lscpu`, `nvidia-smi`, RAM,
   compiler, flags) and every benchmark CSV gains a hardware-id column; `reports/` is un-gitignored
   so evaluators can see it ([[missing-hardware-metadata-in-evidence]]).
4. **Statistical hygiene:** runners gain `--repeat 3` and emit median + spread; single-shot timings
   stop being reported as results ([[benchmark-suites]]).

## 16.6 Definition of Done — requirement → proof

| R# | Proven by (test / command / artifact) |
|---|---|
| R1, R10 | `ctest -R sovereignty_guard` in all 8 CI jobs + `scripts/check-sovereignty.py` + `PROVENANCE.md` |
| R2 | `ctest -R "primal_simplex\|milp\|qp"` and `cli_qp_portfolio` green |
| R4 | `ctest -R ipm_crossover` green, **or** documented re-scope with evidence in `STATUS.md` |
| R5 | `ctest -R cut_effectiveness` + `evidence/benchmarks/cut_reduction.csv` |
| R6 | `ctest -R "sparse_basis\|dense_lu\|sparse_canonicalize"` |
| R7 | `ctest -R parallel_scaling` + `evidence/benchmarks/parallel_scaling.csv` (E5) |
| R8 | `ctest -R gpu_status_gate` + `evidence/benchmarks/crossover_study.csv` with hardware manifest (E4) |
| R9 | `ctest -R golden_results` + `iterative_refinement` + `docs/decisions/ADR-M0-03-numerical-policy.md` |
| R12 | `ctest -R scale_path` on a ≥ 10k-row instance |
| R13, R17 | `ctest -R robustness_suite` + `evidence/robustness-dossier.md` (E3) |
| R14 | `ctest -R "cli_certificate\|json_schema"` + `install()` smoke command |
| R15, R19 | `ctest -R "netlib_benchmarks\|miplib_benchmarks\|mittelmann_benchmarks\|qplib_benchmarks"` |
| R16, R20 | `ctest -R compare_harness` + `evidence/comparison/table.csv` with GM column (E1) |
| R18 | claims↔evidence table in `STATUS.md`; every claim row cites an artifact path |

## Related

[[12-keep-remove-rebuild]] · [[13-restart-point]] · [[14-target-architecture]] · [[testing-gaps]] · [[benchmark-suites]] · [[04-evidence-inventory]] · [[No External Baseline]] · [[Ill-Conditioned Instance Dossier]]
