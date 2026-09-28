# Comprehensive solver comparison (W9 / M6-37 multi-solver + Dolan-More)

- Generated: 2026-09-28T10:49:42 by `scripts/run_full_compare.py` (milestone-6 feature 37, Gap 6)
- Host cores: 12; harness wall clock: 3.1 min
- markov-cero binary: `build_plan/markov-cero-solve` (version 0.5.2, engine auto)
- markov-cero SHA-256: `beaed56a7534f8c8482819c1ad78aae69951c7b7ea98ef54d188bc43258e7cd1`
- Per-instance per-solver time cap: 15 s
- Solver threads per run: 1
- Curated instances: 23 (miplib 5, mittelmann 4, netlib 10, qp 4)
- Results CSV: `evidence/comparison/current_glpk_pinned_20260928/full_compare_results.csv` (115 rows)
- Dolan-More SVG: `evidence/comparison/current_glpk_pinned_20260928/dolan_more_runtime_profile.svg`
- Dolan-More data: `evidence/comparison/current_glpk_pinned_20260928/dolan_more_profile_data.csv`
- Suite exports: `evidence/comparison/current_glpk_pinned_20260928/netlib_lp_comparison.csv`, `evidence/comparison/current_glpk_pinned_20260928/miplib_comparison.csv`, `evidence/comparison/current_glpk_pinned_20260928/mittelmann_lp_comparison.csv`, `evidence/comparison/current_glpk_pinned_20260928/mittelmann_milp_comparison.csv`, `evidence/comparison/current_glpk_pinned_20260928/qplib_comparison.csv`, `evidence/comparison/current_glpk_pinned_20260928/dolan_more_lp.svg`, `evidence/comparison/current_glpk_pinned_20260928/dolan_more_milp.svg`.

## 1. Solver availability

| solver | available | version | probe | notes |
|---|---|---|---|---|
| markov-cero | yes | 0.5.2 | binary `--help` probe | engine selection: auto; CLI solver |
| HiGHS | yes | 1.15.1 | import highspy (in-process API) | - |
| GLPK | yes | GLPSOL--GLPK LP/MIP Solver 5.0 | /home/saikrishna/markov-initial-build/.venv/bin/glpsol | external glpsol process; GLPK uses one thread |
| CBC | yes | 2.10.3 | pulp PULP_CBC_CMD (bundled CBC binary) | pulp 3.3.2 + CBC 2.10.3 |
| SCIP | yes | 10.0.2 (PySCIPOpt 6.2.1) | import pyscipopt (in-process API) | - |

Every solver is optional: a solver that cannot run on this host is reported here as unavailable and produces no result rows. GLPK runs as an external, serial `glpsol` process when available.

## 2. Aggregate results

| solver | rows | optimal | feasible | timeout/error | unsupported | verified | disagreements | geomean(t / t_markov) |
|---|---|---|---|---|---|---|---|---|
| markov-cero | 23 | 17 | 0 | 6 | 0 | 17 | 0 | 1.000 (self) |
| HiGHS | 23 | 17 | 6 | 0 | 0 | 17 | 0 | 0.400 |
| GLPK | 23 | 14 | 5 | 0 | 4 | 14 | 0 | 0.185 |
| CBC | 23 | 13 | 6 | 0 | 4 | 13 | 0 | 0.331 |
| SCIP | 23 | 18 | 5 | 0 | 0 | 18 | 0 | 0.128 |

- Total rows: 115; optimal runs: 79/115; verified objectives: 79/115
- **Disagreements found: 0** (pairs of optimal objectives whose relative difference exceeds 0.0001)
  (no pair of optimal objectives disagreed beyond the 0.0001 relative tolerance)

`geomean(t / t_markov)` is the geometric mean of `runtime(solver) / runtime(markov-cero)` over the instances where both solvers reached `Optimal`; values above 1 mean the solver is slower than markov-cero. The inverse direction (`markov-cero / solver`, the convention used by the legacy W9 report) is simply `1 / value`.

## 3. Per-instance results

Cell format: `status · objective · runtime · verified` (`-` = no certified objective).

| instance | class | markov-cero | HiGHS | GLPK | CBC | SCIP |
|---|---|---|---|---|---|---|
| afiro | LP | Optimal · -464.753 · 3.7 ms · yes | Optimal · -464.753 · 9.5 ms · yes | Optimal · -464.753 · 12.4 ms · yes | Optimal · -464.753 · 11.3 ms · yes | Optimal · -464.753 · 1.4 ms · yes |
| adlittle | LP | Optimal · 225495 · 23.8 ms · yes | Optimal · 225495 · 15.1 ms · yes | Optimal · 225495 · 13.6 ms · yes | Optimal · 225495 · 18.6 ms · yes | Optimal · 225495 · 2.9 ms · yes |
| sc50a | LP | Optimal · -64.5751 · 8.3 ms · yes | Optimal · -64.5751 · 5.7 ms · yes | Optimal · -64.5751 · 6.4 ms · yes | Optimal · -64.5751 · 6 ms · yes | Optimal · -64.5751 · 0.9 ms · yes |
| blend | LP | Optimal · -30.8121 · 31 ms · yes | Optimal · -30.8121 · 8 ms · yes | Optimal · -30.8121 · 3.5 ms · yes | Optimal · -30.8121 · 6.3 ms · yes | Optimal · -30.8121 · 2.6 ms · yes |
| lotfi | LP | Optimal · -25.2647 · 93.1 ms · yes | Optimal · -25.2647 · 19.4 ms · yes | Optimal · -25.2647 · 5.5 ms · yes | Optimal · -25.2647 · 21.9 ms · yes | Optimal · -25.2647 · 4.2 ms · yes |
| kb2 | LP | Optimal · -1749.9 · 7.7 ms · yes | Optimal · -1749.9 · 7.9 ms · yes | Optimal · -1749.9 · 7.2 ms · yes | Optimal · -1749.9 · 6.1 ms · yes | Optimal · -1749.9 · 1.5 ms · yes |
| scorpion | LP | Optimal · 1878.12 · 526.7 ms · yes | Optimal · 1878.12 · 18.8 ms · yes | Optimal · 1878.12 · 8.6 ms · yes | Optimal · 1878.12 · 15.5 ms · yes | Optimal · 1878.12 · 4.8 ms · yes |
| share2b | LP | Optimal · -415.732 · 23.7 ms · yes | Optimal · -415.732 · 9.7 ms · yes | Optimal · -415.732 · 3.6 ms · yes | Optimal · -415.732 · 13.3 ms · yes | Optimal · -415.732 · 3.3 ms · yes |
| beaconfd | LP | Optimal · 33592.5 · 33.4 ms · yes | Optimal · 33592.5 · 21.9 ms · yes | Optimal · 33592.5 · 13.8 ms · yes | Optimal · 33592.5 · 24.8 ms · yes | Optimal · 33592.5 · 3.1 ms · yes |
| recipe | LP | Optimal · -266.616 · 14.8 ms · yes | Optimal · -266.616 · 13.1 ms · yes | Optimal · -266.616 · 11.3 ms · yes | Optimal · -266.616 · 15.5 ms · yes | Optimal · -266.616 · 4.6 ms · yes |
| stein9 | MILP | Optimal · 5 · 25.2 ms · yes | Optimal · 5 · 17.1 ms · yes | Optimal · 5 · 14.9 ms · yes | Optimal · 5 · 18.9 ms · yes | Optimal · 5 · 9.2 ms · yes |
| stein15 | MILP | Optimal · 9 · 1084.6 ms · yes | Optimal · 9 · 46.3 ms · yes | Optimal · 9 · 34.4 ms · yes | Optimal · 9 · 167.4 ms · yes | Optimal · 9 · 23.8 ms · yes |
| flugpl | MILP | Optimal · 1.2015e+06 · 5274.3 ms · yes | Optimal · 1.2015e+06 · 75.3 ms · yes | Optimal · 1.2015e+06 · 11.5 ms · yes | Optimal · 1.2015e+06 · 30.7 ms · yes | Optimal · 1.2015e+06 · 10 ms · yes |
| pk1 | MILP | TimeLimit · - · 15018.1 ms · no | Feasible · 27 · 15025.4 ms · no | Feasible · 18 · 15026.9 ms · no | Feasible · 11 · 15058.7 ms · no | Feasible · 16 · 15001.2 ms · no |
| swath1 | MILP | IterationLimit · - · 11705.8 ms · no | Feasible · 383.033 · 15038.4 ms · no | Optimal · 379.071 · 11655.1 ms · yes | Feasible · 379.071 · 15220.7 ms · no | Optimal · 379.071 · 11649.1 ms · yes |
| markshare_5_0 | MILP | TimeLimit · - · 15035.9 ms · no | Feasible · 18 · 15000.8 ms · no | Feasible · 8 · 30033.9 ms · no | Feasible · 11 · 15096.1 ms · no | Feasible · 8 · 15001.5 ms · no |
| bienst1 | MILP | TimeLimit · - · 20757.1 ms · no | Feasible · 69.5 · 15026.8 ms · no | Feasible · 46.75 · 15023.8 ms · no | Feasible · 46.75 · 30055.5 ms · no | Feasible · 46.75 · 15000.9 ms · no |
| neos5 | MILP | TimeLimit · - · 15004 ms · no | Feasible · 20 · 15018.4 ms · no | Feasible · 15 · 15016 ms · no | Feasible · 15 · 15064.5 ms · no | Feasible · 15 · 15001.4 ms · no |
| ran14x18_1 | MILP | TimeLimit · - · 15032.1 ms · no | Feasible · 4265 · 15031.7 ms · no | Feasible · 4290 · 15012.2 ms · no | Feasible · 3820 · 15037.6 ms · no | Feasible · 3844 · 15002 ms · no |
| QPLIB_0001 | QP | Optimal · -3.5 · 13.3 ms · yes | Optimal · -3.5 · 12.5 ms · yes | Unsupported · - · 0 ms · no | Unsupported · - · 0 ms · no | Optimal · -3.5 · 10.6 ms · yes |
| QPLIB_0002 | QP | Optimal · -2.10715 · 34.4 ms · yes | Optimal · -2.10714 · 34.3 ms · yes | Unsupported · - · 0 ms · no | Unsupported · - · 0 ms · no | Optimal · -2.10714 · 31.2 ms · yes |
| QPLIB_0010 | QP | Optimal · -0.103342 · 33.2 ms · yes | Optimal · -0.103342 · 32.3 ms · yes | Unsupported · - · 0 ms · no | Unsupported · - · 0 ms · no | Optimal · -0.103342 · 30.1 ms · yes |
| QPLIB_0025 | QP | Optimal · 2681 · 7.6 ms · yes | Optimal · 2681 · 6.5 ms · yes | Unsupported · - · 0 ms · no | Unsupported · - · 0 ms · no | Optimal · 2681 · 3.7 ms · yes |

## 4. Dolan-More performance profile

Generated by `scripts/dolan_more_profile.py` (hand-rolled SVG, pure Python, no matplotlib):

- `evidence/comparison/current_glpk_pinned_20260928/dolan_more_runtime_profile.svg`
- `evidence/comparison/current_glpk_pinned_20260928/dolan_more_profile_data.csv` (sampled `tau, solver, rho` points)

Exact formulation used (no approximation, no smoothing):

    r(p,s) = t(p,s) / min over s' of t(p,s'),   min over solvers s' that solved p
    r(p,s) = +inf when solver s failed or timed out on p
    if s solved p and no other solver did, min = t(p,s) so r(p,s) = 1 exactly
    rho_s(tau) = (1 / |P|) * |{ p in P : r(p,s) <= tau }|

`|P| = 23` curated problems (problems nobody solved stay in the denominator and contribute 0 to every curve). A solver is a "success" on p exactly when its status in `full_compare_results.csv` is `Optimal`; timeouts, feasible-but-unproven incumbents, errors and unsupported models are failures with `r = +inf`. The x axis is log-scaled (`tau` from 1 to 100, decades), the y axis is `rho_s(tau)` from 0 to 1, one polyline per solver with a legend.

## 5. Mittelmann published-reference cross-check

Reference document: `evidence/comparison/current_glpk_pinned_20260928/mittelmann_reference.md` (published Mittelmann tables, regenerated by this script and extended with per-instance entries). The table below compares the markov-cero objectives on the `data/mittelmann` instances against the reference entries **where instance names match**.

| instance | markov-cero status | markov-cero objective | reference entry (name match) | reference objective | objective rel-diff | verification |
|---|---|---|---|---|---|---|
| markshare_5_0 | TimeLimit | - | no exact name match | - | n/a | unverified: published tables record wall-clock times only; no published objective for this instance |
| bienst1 | TimeLimit | - | no exact name match | - | n/a | unverified: published tables record wall-clock times only; no published objective for this instance |
| neos5 | TimeLimit | - | p_neos5 (milp_12threads.csv) | - | n/a | unverified: published tables record wall-clock times only; no published objective for this instance |
| ran14x18_1 | TimeLimit | - | no exact name match | - | n/a | unverified: published tables record wall-clock times only; no published objective for this instance |

Measured cross-check from this run (not a published reference):

- **markshare_5_0**: no solver proved optimality within 15 s (all rows are timeouts/errors)
- **bienst1**: no solver proved optimality within 15 s (all rows are timeouts/errors)
- **neos5**: no solver proved optimality within 15 s (all rows are timeouts/errors)
- **ran14x18_1**: no solver proved optimality within 15 s (all rows are timeouts/errors)

Reference objectives for the `data/mittelmann` set are **absent** in every source in this repository (provenance files carry no `reference_objective`, and the published Mittelmann tables only publish wall-clock times), so every row above is honestly marked unverified. Where the published table does contain a name-matched entry (`p_neos5`), its times are cited verbatim in `mittelmann_reference.md`.

## 6. Methodology, gaps and preserved artifacts

- Measured runs share the curated instance set, the time cap (15 s), and the host. Published Mittelmann numbers are cited, never mixed into the measured geomeans. Failures are reported, never dropped.
- The solvers of one instance run concurrently (pool cap 5 workers for 5 available solvers); each configurable solver uses 1 thread(s), while GLPK is serial. Wall clock of the whole matrix: 3.1 min.
- Timing asymmetry (inherited from the W9 harness): markov-cero is timed as a full subprocess (spawn + model read + solve), the Python API solvers are timed over model load + solve.
- QP inputs: markov-cero and HiGHS read `QUADOBJ` MPS directly; CBC and GLPK cannot handle quadratic objectives (rows are `Unsupported`); SCIP's MPS reader requires `QUADOBJ` after `BOUNDS` and an `RHS` section, so for SCIP only the file is reformatted into a temporary copy (sections reordered, empty `RHS` injected when missing) - the mathematics of the file is unchanged.
- `verified` = status `Optimal` AND relative agreement (0.0001) with an independent objective: the provenance `reference_objective` when the instance has one, otherwise markov-cero's certificate-backed objective. `verified=no` therefore means "not independently checked", not "wrong".
- Preserved untouched: `evidence/comparison/netlib_comparison.csv` and `evidence/comparison/miplib_comparison.csv` (W9 legacy schema, add-don't-break); `evidence/comparison/current_glpk_pinned_20260928/full_compare_results.csv` supersedes them for the curated multi-solver set.


