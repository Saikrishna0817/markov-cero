# Comprehensive solver comparison (W9 / M6-37 multi-solver + Dolan-More)

- Generated: 2026-09-28T06:14:04 by `scripts/run_full_compare.py` (milestone-6 feature 37, Gap 6)
- Host cores: 12; harness wall clock: 4.0 min
- markov-cero binary: `build_ml/markov-cero-solve` (version 0.5.2, engine auto)
- Per-instance per-solver time cap: 15 s
- Solver threads per run: 1
- Curated instances: 23 (miplib 5, mittelmann 4, netlib 10, qp 4)
- Results CSV: `evidence/comparison/current_build_thread1_20260928/full_compare_results.csv` (92 rows)
- Dolan-More SVG: `evidence/comparison/current_build_thread1_20260928/dolan_more_runtime_profile.svg`
- Dolan-More data: `evidence/comparison/current_build_thread1_20260928/dolan_more_profile_data.csv`
- Suite exports: `evidence/comparison/current_build_thread1_20260928/netlib_lp_comparison.csv`, `evidence/comparison/current_build_thread1_20260928/miplib_comparison.csv`, `evidence/comparison/current_build_thread1_20260928/mittelmann_lp_comparison.csv`, `evidence/comparison/current_build_thread1_20260928/mittelmann_milp_comparison.csv`, `evidence/comparison/current_build_thread1_20260928/qplib_comparison.csv`, `evidence/comparison/current_build_thread1_20260928/dolan_more_lp.svg`, `evidence/comparison/current_build_thread1_20260928/dolan_more_milp.svg`.

## 1. Solver availability

| solver | available | version | probe | notes |
|---|---|---|---|---|
| markov-cero | yes | 0.5.2 | binary `--help` probe | engine selection: auto; CLI solver |
| HiGHS | yes | 1.15.1 | import highspy (in-process API) | - |
| CBC | yes | 2.10.3 | pulp PULP_CBC_CMD (bundled CBC binary) | pulp 3.3.2 + CBC 2.10.3 |
| SCIP | yes | 10.0.2 (PySCIPOpt 6.2.1) | import pyscipopt (in-process API) | - |
| GLPK | **no** | unknown | which glpsol | unavailable: no system libglpk / glpsol on this host and system packages cannot be installed (no sudo); no bundled GLPK wheel |

Every solver is optional: a solver that cannot run on this host is reported here as unavailable and simply produces no rows in `full_compare_results.csv`. GLPK stays documented as unavailable because there is no system `libglpk`/`glpsol` and system packages cannot be installed without sudo.

## 2. Aggregate results

| solver | rows | optimal | feasible | timeout/error | unsupported | verified | disagreements | geomean(t / t_markov) |
|---|---|---|---|---|---|---|---|---|
| markov-cero | 23 | 16 | 0 | 7 | 0 | 16 | 0 | 1.000 (self) |
| HiGHS | 23 | 17 | 6 | 0 | 0 | 17 | 0 | 0.081 |
| CBC | 23 | 13 | 6 | 0 | 4 | 13 | 0 | 0.072 |
| SCIP | 23 | 18 | 5 | 0 | 0 | 18 | 0 | 0.049 |

- Total rows: 92; optimal runs: 64/92; verified objectives: 64/92
- **Disagreements found: 0** (pairs of optimal objectives whose relative difference exceeds 0.0001)
  (no pair of optimal objectives disagreed beyond the 0.0001 relative tolerance)

`geomean(t / t_markov)` is the geometric mean of `runtime(solver) / runtime(markov-cero)` over the instances where both solvers reached `Optimal`; values above 1 mean the solver is slower than markov-cero. The inverse direction (`markov-cero / solver`, the convention used by the legacy W9 report) is simply `1 / value`.

## 3. Per-instance results

Cell format: `status · objective · runtime · verified` (`-` = no certified objective).

| instance | class | markov-cero | HiGHS | CBC | SCIP |
|---|---|---|---|---|---|
| afiro | LP | Optimal · -464.753 · 8.7 ms · yes | Optimal · -464.753 · 7.1 ms · yes | Optimal · -464.753 · 9 ms · yes | Optimal · -464.753 · 1.1 ms · yes |
| adlittle | LP | Optimal · 225495 · 147.9 ms · yes | Optimal · 225495 · 12.6 ms · yes | Optimal · 225495 · 14.6 ms · yes | Optimal · 225495 · 3.1 ms · yes |
| sc50a | LP | Optimal · -64.5751 · 18.6 ms · yes | Optimal · -64.5751 · 7.9 ms · yes | Optimal · -64.5751 · 4.6 ms · yes | Optimal · -64.5751 · 0.8 ms · yes |
| blend | LP | Optimal · -30.8121 · 157.8 ms · yes | Optimal · -30.8121 · 10.3 ms · yes | Optimal · -30.8121 · 5.7 ms · yes | Optimal · -30.8121 · 4.4 ms · yes |
| lotfi | LP | Optimal · -25.2647 · 925.3 ms · yes | Optimal · -25.2647 · 15.7 ms · yes | Optimal · -25.2647 · 13.7 ms · yes | Optimal · -25.2647 · 4.8 ms · yes |
| kb2 | LP | Optimal · -1749.9 · 33.7 ms · yes | Optimal · -1749.9 · 8.4 ms · yes | Optimal · -1749.9 · 4.4 ms · yes | Optimal · -1749.9 · 1.2 ms · yes |
| scorpion | LP | Optimal · 1878.12 · 10068.1 ms · yes | Optimal · 1878.12 · 20.5 ms · yes | Optimal · 1878.12 · 14.2 ms · yes | Optimal · 1878.12 · 5.1 ms · yes |
| share2b | LP | Optimal · -415.732 · 145.2 ms · yes | Optimal · -415.732 · 15.1 ms · yes | Optimal · -415.732 · 10.7 ms · yes | Optimal · -415.732 · 3.5 ms · yes |
| beaconfd | LP | Optimal · 33592.5 · 147 ms · yes | Optimal · 33592.5 · 25.4 ms · yes | Optimal · 33592.5 · 24.6 ms · yes | Optimal · 33592.5 · 3.2 ms · yes |
| recipe | LP | Optimal · -266.616 · 103.9 ms · yes | Optimal · -266.616 · 17.3 ms · yes | Optimal · -266.616 · 17 ms · yes | Optimal · -266.616 · 7.4 ms · yes |
| stein9 | MILP | Optimal · 5 · 137.4 ms · yes | Optimal · 5 · 17.5 ms · yes | Optimal · 5 · 19.6 ms · yes | Optimal · 5 · 9.3 ms · yes |
| stein15 | MILP | Optimal · 9 · 7617.4 ms · yes | Optimal · 9 · 48.8 ms · yes | Optimal · 9 · 190.6 ms · yes | Optimal · 9 · 27.4 ms · yes |
| flugpl | MILP | TimeLimit · - · 15013.1 ms · no | Optimal · 1.2015e+06 · 88.9 ms · yes | Optimal · 1.2015e+06 · 33.5 ms · yes | Optimal · 1.2015e+06 · 10.3 ms · yes |
| pk1 | MILP | TimeLimit · - · 15132.1 ms · no | Feasible · 27 · 15021.7 ms · no | Feasible · 11 · 15044.4 ms · no | Feasible · 16 · 15000.6 ms · no |
| swath1 | MILP | IterationLimit · - · 13384.7 ms · no | Feasible · 406.334 · 15045.1 ms · no | Feasible · 379.071 · 15223.2 ms · no | Optimal · 379.071 · 13336.2 ms · yes |
| markshare_5_0 | MILP | TimeLimit · - · 15089.4 ms · no | Feasible · 202 · 15032.4 ms · no | Feasible · 11 · 15132.3 ms · no | Feasible · 8 · 15000.5 ms · no |
| bienst1 | MILP | TimeLimit · - · 35016.4 ms · no | Feasible · 69.5 · 15009.8 ms · no | Feasible · 46.75 · 15042.5 ms · no | Feasible · 46.75 · 15001 ms · no |
| neos5 | MILP | TimeLimit · - · 29393.1 ms · no | Feasible · 20 · 15018.4 ms · no | Feasible · 15 · 15066.5 ms · no | Feasible · 15 · 15000.7 ms · no |
| ran14x18_1 | MILP | TimeLimit · - · 50052.5 ms · no | Feasible · 3748 · 30005.3 ms · no | Feasible · 3820 · 15047.3 ms · no | Feasible · 3844 · 15000.9 ms · no |
| QPLIB_0001 | QP | Optimal · -3.5 · 16.7 ms · yes | Optimal · -3.5 · 0.8 ms · yes | Unsupported · - · 0 ms · no | Optimal · -3.5 · 12.5 ms · yes |
| QPLIB_0002 | QP | Optimal · -2.10715 · 35.3 ms · yes | Optimal · -2.10714 · 0.7 ms · yes | Unsupported · - · 0 ms · no | Optimal · -2.10714 · 32.3 ms · yes |
| QPLIB_0010 | QP | Optimal · -0.103342 · 34.8 ms · yes | Optimal · -0.103342 · 33.1 ms · yes | Unsupported · - · 0 ms · no | Optimal · -0.103342 · 31.4 ms · yes |
| QPLIB_0025 | QP | Optimal · 2681 · 7 ms · yes | Optimal · 2681 · 0.6 ms · yes | Unsupported · - · 0 ms · no | Optimal · 2681 · 4.3 ms · yes |

## 4. Dolan-More performance profile

Generated by `scripts/dolan_more_profile.py` (hand-rolled SVG, pure Python, no matplotlib):

- `evidence/comparison/current_build_thread1_20260928/dolan_more_runtime_profile.svg`
- `evidence/comparison/current_build_thread1_20260928/dolan_more_profile_data.csv` (sampled `tau, solver, rho` points)

Exact formulation used (no approximation, no smoothing):

    r(p,s) = t(p,s) / min over s' of t(p,s'),   min over solvers s' that solved p
    r(p,s) = +inf when solver s failed or timed out on p
    if s solved p and no other solver did, min = t(p,s) so r(p,s) = 1 exactly
    rho_s(tau) = (1 / |P|) * |{ p in P : r(p,s) <= tau }|

`|P| = 23` curated problems (problems nobody solved stay in the denominator and contribute 0 to every curve). A solver is a "success" on p exactly when its status in `full_compare_results.csv` is `Optimal`; timeouts, feasible-but-unproven incumbents, errors and unsupported models are failures with `r = +inf`. The x axis is log-scaled (`tau` from 1 to 100, decades), the y axis is `rho_s(tau)` from 0 to 1, one polyline per solver with a legend.

## 5. Mittelmann published-reference cross-check

Reference document: `evidence/comparison/current_build_thread1_20260928/mittelmann_reference.md` (published Mittelmann tables, regenerated by this script and extended with per-instance entries). The table below compares the markov-cero objectives on the `data/mittelmann` instances against the reference entries **where instance names match**.

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
- The solvers of one instance run concurrently (pool cap 4 workers for 4 available solvers); each solver is configured for 1 thread(s). Wall clock of the whole matrix: 4.0 min.
- Timing asymmetry (inherited from the W9 harness): markov-cero is timed as a full subprocess (spawn + model read + solve), the Python API solvers are timed over model load + solve.
- QP inputs: markov-cero and HiGHS read `QUADOBJ` MPS directly; CBC cannot handle quadratic objectives (rows are `Unsupported`); SCIP's MPS reader requires `QUADOBJ` after `BOUNDS` and an `RHS` section, so for SCIP only the file is reformatted into a temporary copy (sections reordered, empty `RHS` injected when missing) - the mathematics of the file is unchanged.
- `verified` = status `Optimal` AND relative agreement (0.0001) with an independent objective: the provenance `reference_objective` when the instance has one, otherwise markov-cero's certificate-backed objective. `verified=no` therefore means "not independently checked", not "wrong".
- Preserved untouched: `evidence/comparison/netlib_comparison.csv` and `evidence/comparison/miplib_comparison.csv` (W9 legacy schema, add-don't-break); `evidence/comparison/current_build_thread1_20260928/full_compare_results.csv` supersedes them for the curated multi-solver set.


