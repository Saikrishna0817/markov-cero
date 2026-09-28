# Comprehensive solver comparison (W9 / M6-37 multi-solver + Dolan-More)

- Generated: 2026-09-28T00:28:17 by `scripts/run_full_compare.py` (milestone-6 feature 37, Gap 6)
- Host cores: 12; harness wall clock: 5.9 min
- markov-cero binary: `build_w5/markov-cero-solve` (version 0.5.2, engine auto)
- Per-instance per-solver time cap: 15 s
- Curated instances: 23 (10 netlib LP, 5 miplib MILP + markshare_5_0 = 6 MILP total, 4 data/mittelmann, 4 QP)
- Results CSV: `evidence/comparison/full_compare_results.csv` (92 rows)
- Dolan-More SVG: `evidence/comparison/dolan_more_runtime_profile.svg`
- Dolan-More data: `evidence/comparison/dolan_more_profile_data.csv`
- Suite exports: `netlib_lp_comparison.csv`, `miplib_comparison.csv`, `mittelmann_lp_comparison.csv`, `mittelmann_milp_comparison.csv`, and `qplib_comparison.csv`; category profiles: `dolan_more_lp.svg` and `dolan_more_milp.svg`.

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
| markov-cero | 23 | 17 | 0 | 6 | 0 | 17 | 0 | 1.000 (self) |
| HiGHS | 23 | 18 | 5 | 0 | 0 | 18 | 0 | 0.091 |
| CBC | 23 | 13 | 6 | 0 | 4 | 13 | 0 | 0.275 |
| SCIP | 23 | 18 | 5 | 0 | 0 | 18 | 0 | 0.261 |

- Total rows: 92; optimal runs: 66/92; verified objectives: 66/92
- **Disagreements found: 0** (pairs of optimal objectives whose relative difference exceeds 0.0001)
  (no pair of optimal objectives disagreed beyond the 0.0001 relative tolerance)

`geomean(t / t_markov)` is the geometric mean of `runtime(solver) / runtime(markov-cero)` over the instances where both solvers reached `Optimal`; values above 1 mean the solver is slower than markov-cero. The inverse direction (`markov-cero / solver`, the convention used by the legacy W9 report) is simply `1 / value`.

## 3. Per-instance results

Cell format: `status · objective · runtime · verified` (`-` = no certified objective).

| instance | class | markov-cero | HiGHS | CBC | SCIP |
|---|---|---|---|---|---|
| afiro | LP | Optimal · -464.753 · 2.2 ms · yes | Optimal · -464.753 · 0.8 ms · yes | Optimal · -464.753 · 3.4 ms · yes | Optimal · -464.753 · 1 ms · yes |
| adlittle | LP | Optimal · 225495 · 16.4 ms · yes | Optimal · 225495 · 1.5 ms · yes | Optimal · 225495 · 8.9 ms · yes | Optimal · 225495 · 2.7 ms · yes |
| sc50a | LP | Optimal · -64.5751 · 4.5 ms · yes | Optimal · -64.5751 · 0.6 ms · yes | Optimal · -64.5751 · 4.2 ms · yes | Optimal · -64.5751 · 0.8 ms · yes |
| blend | LP | Optimal · -30.8121 · 25.2 ms · yes | Optimal · -30.8121 · 1.5 ms · yes | Optimal · -30.8121 · 5.4 ms · yes | Optimal · -30.8121 · 2.5 ms · yes |
| lotfi | LP | Optimal · -25.2647 · 84.7 ms · yes | Optimal · -25.2647 · 2.3 ms · yes | Optimal · -25.2647 · 9.8 ms · yes | Optimal · -25.2647 · 4.2 ms · yes |
| kb2 | LP | Optimal · -1749.9 · 5.9 ms · yes | Optimal · -1749.9 · 0.8 ms · yes | Optimal · -1749.9 · 5.1 ms · yes | Optimal · -1749.9 · 1.2 ms · yes |
| scorpion | LP | Optimal · 1878.12 · 567.2 ms · yes | Optimal · 1878.12 · 2.7 ms · yes | Optimal · 1878.12 · 16 ms · yes | Optimal · 1878.12 · 4.4 ms · yes |
| share2b | LP | Optimal · -415.732 · 17.8 ms · yes | Optimal · -415.732 · 1.7 ms · yes | Optimal · -415.732 · 9 ms · yes | Optimal · -415.732 · 3.3 ms · yes |
| beaconfd | LP | Optimal · 33592.5 · 17.5 ms · yes | Optimal · 33592.5 · 3.5 ms · yes | Optimal · 33592.5 · 18.8 ms · yes | Optimal · 33592.5 · 3.1 ms · yes |
| recipe | LP | Optimal · -266.616 · 13 ms · yes | Optimal · -266.616 · 1.3 ms · yes | Optimal · -266.616 · 7.3 ms · yes | Optimal · -266.616 · 4.2 ms · yes |
| stein9 | MILP | Optimal · 5 · 26.8 ms · yes | Optimal · 5 · 11.4 ms · yes | Optimal · 5 · 7.8 ms · yes | Optimal · 5 · 8.8 ms · yes |
| stein15 | MILP | Optimal · 9 · 1112.5 ms · yes | Optimal · 9 · 25.2 ms · yes | Optimal · 9 · 130.3 ms · yes | Optimal · 9 · 23.5 ms · yes |
| flugpl | MILP | Optimal · 1.2015e+06 · 2639.3 ms · yes | Optimal · 1.2015e+06 · 73.3 ms · yes | Optimal · 1.2015e+06 · 27.1 ms · yes | Optimal · 1.2015e+06 · 10.5 ms · yes |
| pk1 | MILP | TimeLimit · - · 15030.3 ms · no | Feasible · 14 · 15002.7 ms · no | Feasible · 11 · 15011.6 ms · no | Feasible · 16 · 15000.4 ms · no |
| swath1 | MILP | NumericalFailure · - · 30.3 ms · no | Optimal · 379.071 · 12096.8 ms · yes | Feasible · 379.071 · 15225.9 ms · no | Optimal · 379.071 · 12490.2 ms · yes |
| markshare_5_0 | MILP | TimeLimit · - · 15016.2 ms · no | Feasible · 18 · 15002 ms · no | Feasible · 3 · 15011.5 ms · no | Feasible · 8 · 15000.5 ms · no |
| bienst1 | MILP | TimeLimit · - · 20346.1 ms · no | Feasible · 46.75 · 15002.4 ms · no | Feasible · 46.75 · 15039 ms · no | Feasible · 46.75 · 15000.8 ms · no |
| neos5 | MILP | TimeLimit · - · 16341.4 ms · no | Feasible · 15 · 15002.1 ms · no | Feasible · 15 · 15039.9 ms · no | Feasible · 15 · 15000.6 ms · no |
| ran14x18_1 | MILP | TimeLimit · - · 15070.7 ms · no | Feasible · 3748 · 15002.9 ms · no | Feasible · 3964 · 15036.3 ms · no | Feasible · 3844 · 15002.4 ms · no |
| QPLIB_0001 | QP | Optimal · -3.5 · 2 ms · yes | Optimal · -3.5 · 0.1 ms · yes | Unsupported · - · 0 ms · no | Optimal · -3.5 · 11.1 ms · yes |
| QPLIB_0002 | QP | Optimal · -2.10715 · 1.8 ms · yes | Optimal · -2.10714 · 0.5 ms · yes | Unsupported · - · 0 ms · no | Optimal · -2.10714 · 42 ms · yes |
| QPLIB_0010 | QP | Optimal · -0.103342 · 1.9 ms · yes | Optimal · -0.103342 · 0.4 ms · yes | Unsupported · - · 0 ms · no | Optimal · -0.103342 · 31.2 ms · yes |
| QPLIB_0025 | QP | Optimal · 2681 · 2.3 ms · yes | Optimal · 2681 · 0.7 ms · yes | Unsupported · - · 0 ms · no | Optimal · 2681 · 4.4 ms · yes |

## 4. Dolan-More performance profile

Generated by `scripts/dolan_more_profile.py` (hand-rolled SVG, pure Python, no matplotlib):

- `evidence/comparison/dolan_more_runtime_profile.svg`
- `evidence/comparison/dolan_more_profile_data.csv` (sampled `tau, solver, rho` points)

Exact formulation used (no approximation, no smoothing):

    r(p,s) = t(p,s) / min over s' of t(p,s'),   min over solvers s' that solved p
    r(p,s) = +inf when solver s failed or timed out on p
    if s solved p and no other solver did, min = t(p,s) so r(p,s) = 1 exactly
    rho_s(tau) = (1 / |P|) * |{ p in P : r(p,s) <= tau }|

`|P| = 23` curated problems (problems nobody solved stay in the denominator and contribute 0 to every curve). A solver is a "success" on p exactly when its status in `full_compare_results.csv` is `Optimal`; timeouts, feasible-but-unproven incumbents, errors and unsupported models are failures with `r = +inf`. The x axis is log-scaled (`tau` from 1 to 100, decades), the y axis is `rho_s(tau)` from 0 to 1, one polyline per solver with a legend.

## 5. Mittelmann published-reference cross-check

Reference document: `evidence/comparison/mittelmann_reference.md` (published Mittelmann tables, regenerated by this script and extended with per-instance entries). The table below compares the markov-cero objectives on the `data/mittelmann` instances against the reference entries **where instance names match**.

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
- The solvers of one instance run concurrently (pool cap 1 workers for 4 available solvers); markov-cero keeps its default 4 search threads, HiGHS uses threads=4 and SCIP parallel/maxnthreads=4 (CBC is single-threaded). Wall clock of the whole matrix: 5.9 min.
- Timing asymmetry (inherited from the W9 harness): markov-cero is timed as a full subprocess (spawn + model read + solve), the Python API solvers are timed over model load + solve.
- QP inputs: markov-cero and HiGHS read `QUADOBJ` MPS directly; CBC cannot handle quadratic objectives (rows are `Unsupported`); SCIP's MPS reader requires `QUADOBJ` after `BOUNDS` and an `RHS` section, so for SCIP only the file is reformatted into a temporary copy (sections reordered, empty `RHS` injected when missing) - the mathematics of the file is unchanged.
- `verified` = status `Optimal` AND relative agreement (0.0001) with an independent objective: the provenance `reference_objective` when the instance has one, otherwise markov-cero's certificate-backed objective. `verified=no` therefore means "not independently checked", not "wrong".
- Preserved untouched: `evidence/comparison/netlib_comparison.csv` and `evidence/comparison/miplib_comparison.csv` (W9 legacy schema, add-don't-break); `evidence/comparison/full_compare_results.csv` supersedes them for the curated multi-solver set.

### Appendix: previous W9 report (kept for continuity)

> # Comprehensive solver comparison (W9)
>
> 
> - markov-cero binary: `build_w5/markov-cero-solve`
> - Local baseline: HiGHS n/a via highspy (external oracle; never linked)
> - Published context: Mittelmann tables (see mittelmann_reference.md, D-09 citation)
> - Instances: 20 (miplib, netlib)
> - markov-cero verified-optimal: 20/20
> - Geometric-mean runtime ratio (markov-cero / HiGHS): 17.32x
> 
> Methodology: measured runs share instances, timeout (120.0s), and
> machine; published Mittelmann numbers are cited, never mixed into the measured
> geomean. Failures are reported, never dropped.

