# Status & Capability Matrix — markov-cero v0.5.2

Single source of truth for solver status, capabilities, CLI options, and prototype boundaries.

Status terms: **implemented** means code exists; **verified** means named correctness
cases passed; **hardware verified** requires a recorded device and binary provenance.
Historical GPU speedup figures against an unreliable simplex baseline, including
29,320×, are quarantined. The current RTX 2050 measurements below compare GPU and
CPU PDLP and show no end-to-end GPU benefit on their measured cases.
The `scale_200` regression is enabled with a checked-in fixture and independently
verified optimum. The LO/LI/INTORG parser interaction is fixed; old benchmark
results still require regeneration. LP/QP verification now checks actual witness
conditions, local NLP results use `LocalOptimal`, and feasible PDLP points without
a finite dual bound use `Feasible`. Linear MILP/convex MIQP can export independently replayable numerical trees.
A failed or exhausted proof budget keeps the tree conclusion unverified; OA
bounds remain solver-trusted.

The 2026-09-28 W01 implementation (roadmap backlog item 4) closes IR-19: persistent
branch-bound deltas, copy-on-write local cuts, shared immutable warm-start bases,
parallel LP/QP bound overlays, and a bounded reference-materialisation comparator
(`include/markov_cero/milp/reference_materialisation.hpp`) that replays recorded
branching steps chronologically against production's delta-chain walk and refuses
over-cap requests before allocating. The current full native run passes 89/89 in
Release, ASan/UBSan and TSan; the wheel passes 20 pytest cases. Full-solver
frontier evidence exists: `Result::max_queued_nodes` peaks at 47–531 on flugpl
caps with queue record bytes in the tens of kilobytes while frontier and RSS
growth track root/worker storage; queue bytes are byte-identical across a 61×
root-matrix growth. Worker-class storage remains by design (serial node-model
workspace, per-worker bound overlays, sparse-PDLP fallback), per-node cut
application and persistent-structure caps remain open, and solve-wide memory
limits stay open under IR-21; A transient ~1 GiB RSS anomaly (5/~35 runs) is
recorded in [evidence](evidence/ir19-w01-memory-20260928.json). The earlier
structure-only comparison remains in
[evidence](evidence/node-frontier-memory-20260928.json). See the
[competitive roadmap](docs/audit/INDUSTRY-GRADE-COMPETITIVE-ROADMAP.md).

The 2026-09-28 backlog-items-5–11 wave (roadmap §14) lands the solve-wide resource
contract: one `SolveContext` per solve threads a deadline, an instrumented memory
budget, worker accounting and a sticky stop reason through every engine, verifier
stage and parallel worker; `--memory-limit-bytes` and solve-wide time limits are
exposed in C++, CLI and Python with `stop_reason` on every result. MIP proofs now
carry assurance tiers, consumed budgets, fingerprint binding and build/replay
timings through C++, CLI JSON and Python; exhausted proof budgets leave the tree
guarantee unverified. Benchmark comparators, instance manifests, family splits and
timing constants are frozen in code and the pre-tuning baseline was regenerated
under pinned hashes. Dual-simplex sessions reuse basis/factorization and QP reuses
symbolic KKT analysis behind shape/fingerprint gates (measured, opt-in). MILP
propagation evidence, serial bottleneck timings and format-3 audit proof
annotations landed; the refinery demo gained a typed units schema, unit-checked
feed/quality enforcement and named unit-labelled reports. Packaging was qualified
offline (wheel/sdist/prefix/consumer, performed rollback drill, support and
rollback docs). Gates stay honest: IR-20/21 remain OPEN (cooperative deadlines,
charge-point-only memory metering), IR-33/G7 and IR-34/G34/G8 remain OPEN
(external review, support ownership, engineer sign-off), and the 100%
critical/high release gate is still unmet. Evidence:
[resource envelope](evidence/resource-envelope-20260928.json),
[proof guarantee](evidence/proof-guarantee-20260928.json),
[baseline freeze](evidence/baseline-freeze-20260928.json),
[repeated solve](evidence/repeated-solve-20260928.json),
[MILP strengthening](evidence/milp-strengthening-20260928.json),
[refinery schema](evidence/refinery-units-schema-20260928.json),
[packaging](evidence/packaging-qualification-20260928.json),
[index](evidence/backlog-5-12-20260928.json).

W02 has a first queued-node cap in serial and parallel MILP, exposed through the
C++ API, CLI and Python options. Exhaustion returns `ResourceLimit` and retains the
minimum certified bound of omitted nodes; it cannot prove infeasibility or
optimality. Parallel branch bounds were corrected to propagate LP lower bounds,
not primal relaxation objectives. This does not yet bound total bytes or cover
parser/factor/proof allocations, allocation failures, device memory or worker
isolation; W02 remains open.

The 2026-09-28 shared-contract slice (roadmap backlog item 2) adds the
`SolveContext` family (`StopReason`, `Deadline`, `MemoryBudget`,
`SolveContext` in `include/markov_cero/core/`), `ModelSnapshot` with
separable structural/numeric hashes and a stable model fingerprint, and
`NodeView` with scoped cut IDs, bound-evidence provenance and budget-charged
materialization. `SolveResult::model_fingerprint` binds every result to the
validated model at the API boundary and is exposed to Python as
`model_fingerprint`. New CTest gates `solve_context`,
`model_snapshot` and `node_view` pass; the current Release run passes 85/85
and a locally built wheel passes its 20 pytest cases. See
[evidence](evidence/contracts-slice-20260928.json).
These are contracts with tests, not yet consumed end to end: engines still
receive the mutable `Model`, no solve-wide byte budget is exposed, and
IR-19/20/21 stay open.

The 2026-09-28 instrumentation slice (roadmap backlog item 3) adds
`StageScope` (`include/markov_cero/core/instrumentation.hpp`), an RAII stage
timer that emits exactly one trace event per stage with its counter, net
charged bytes, observed duration, shared stop reason and worker index, plus
`SolveContext::remaining_ms()` for live deadline slack. It defines the worker
isolation boundary in `include/markov_cero/core/worker_context.hpp`: workers
share one context, one memory budget and the solve-wide stop reason, while
charge attribution, release rights and a worker-local stop reason stay private,
so a worker can stop itself without stopping its siblings and can never
under-count another worker's bytes. An allocation-failure harness
(`tests/support/failing_new.hpp`, `tests/resource_failure_test.cpp`) replaces
global `operator new`/`delete` in one test binary and fails the nth allocation
of a call under test: `ModelSnapshot::capture` reports the failure and recovers,
`NodeView::materialize` returns a new `MaterializationStatus::allocation_failed`
(distinct from budget exhaustion) with its charge released and outputs
untouched, and the API boundary maps every injected failure to `resource_limit`
with failure site `allocation_failure` — zero exceptions escaping
`solve_file`/`solve_model`, never an infeasible or unbounded status.
`src/api/api.cpp` now catches `std::bad_alloc` (it previously fell through to
`numerical_failure`). New CTest gates `worker_context` and `resource_failure`
bring the suite to 87/87 in Release, under ASan/UBSan with warnings-as-errors
and under TSan; the wheel passes 20 pytest cases. See
[evidence](evidence/resource-instrumentation-20260928.json). Still open: no
engine stage emits stage events, the API creates no `SolveContext` yet, the
parallel search does not use `WorkerContext`, injection coverage stops at the
exercised boundary points, and there is no worker-kill or peak-RSS evidence —
IR-20/21 stay open.


The synthetic refinery LP validates declared canonical units, accepts dimension-checked typed
inputs, enforces gasoline RON and fuel-oil sulfur, checks FCC feed balance, and emits named
unit-labelled reports including duals, slacks, quality margins and row-scoped IIS entries. Legacy
raw double inputs assume canonical units. This is synthetic/public-data qualification, not a plant
model. Refinery engineer approval (IR-34/G34) and shadow-trial agreement (G8) were NOT obtained;
the industrial refinery gate remains OPEN. Debug CPU and a fresh
ML-enabled Release build each pass 80/80 CTests; the installed wheel passes 16/16.
All maintained code/build files meet the 300-line limit. See
[evidence](evidence/readiness-checkpoint.json) and the
[closure register](evidence/defect-closure-register.csv): 29/32 actionable code
defects closed (90.6%), with 3 open engineering defects and 4 separately listed
capability/release gates. This does not satisfy the 100% critical/high release gate.
See the [implementation plan](docs/audit/INDUSTRY-READINESS-IMPLEMENTATION-PLAN.md).

---

## 1. Implemented Capabilities

| Capability | Status | Implementation / Evidence |
|---|---|---|
| Free-format MPS parser | Implemented; LO/LI regression corrected | `src/io/mps.cpp`, fuzz targets, edge tests; benchmark regeneration pending |
| Immutable model (CSC) | Implemented | `src/model/model.cpp` |
| Sparse canonical model | Implemented | `src/transform/sparse_canonicalize.cpp` |
| Primal revised simplex | Implemented | `src/lp/reference/revised_simplex.cpp`, Netlib pass |
| Dual warm simplex | Implemented | `src/lp/dual/dual_simplex.cpp`, basis serialization |
| Sparse basis LU + Eta updates | Implemented | `src/linalg/sparse_basis.cpp`, property tests |
| Matrix-free PDLP (CPU) | Implemented | `src/lp/first_order/pdlp.cpp`, `tests/pdlp_test.cpp` |
| Sovereign MILP Branch-and-Cut | Implemented | `src/milp/milp_solver.cpp`, 3/3 MIPLIB pass |
| Parallel tree search | Implemented; historical flugpl scaling evidence only; current broad scaling and TSan revalidation remain open | `src/milp/parallel_tree_search.cpp`, C++20 jthread, `evidence/benchmarks/rw2_parallel_scaling.md` |
| Gomory Mixed-Integer (GMI) cuts | Implemented | `src/milp/gomory.cpp` with slack substitution |
| Mixed-Integer Rounding (MIR) cuts | Implemented | `src/milp/mir.cpp`, cosine filtering |
| Strong branching | Implemented | `src/milp/strong_branching.cpp`, pseudo-costs |
| Primal heuristics & pump | Implemented | `src/milp/heuristics.cpp` with cycle perturbation |
| Reversible presolve / postsolve | Implemented | `src/presolve/presolve.cpp`, `presolve_test.cpp` |
| Ruiz matrix scaling | Implemented | `src/scale/ruiz_scaling.cpp`, `ruiz_scaling_test.cpp` |
| Independent canonical verifier | Implemented | `src/verify/reference_lp_verifier.cpp` |
| Original primal verifier | Implemented | `src/verify/primal_verifier.cpp` |
| Solve CLI & JSON output | Implemented | `apps/markov_cero_solve.cpp` |
| Refinery qualification demo | Implemented; unit schema + named reports added | `examples/refinery/`, `run-qualification-demo.sh`; engineer approval and shadow trial remain open (IR-34) |
| Repeated LP/QP solves (item 8) | Session APIs implemented; engine wiring opt-in | Verified 100-step RHS/bounds benchmarks, cache invalidation tests, build_item8 CTest 89/89; `evidence/repeated-solve-20260928.json` |
| Solve-wide resource contract (item 5) | Implemented; IR-20/21 remain OPEN (envelope, not closure) | `evidence/resource-envelope-20260928.json`; cooperative deadlines, charge-point-only memory metering |
| MIP proof guarantees (item 6) | Implemented (tiers, budgets, fingerprint, timings) | `evidence/proof-guarantee-20260928.json`; exhausted budgets stay unverified |
| Packaging qualification (item 11, code) | Implemented locally; IR-33/G7 OPEN | `evidence/packaging-qualification-20260928.json`, `docs/governance/support-rollback.md` |
| Benchmark baseline freeze (item 7) | Implemented; regenerated pinned baseline | `evidence/baseline-freeze-20260928.json`, `evidence/comparison/baseline_frozen_20260928/` |
| GPU PDLP acceleration | Two additional RTX 2050 host runs each verified all four scales in opposite instance orders. GPU was 2.3–8.3× slower end-to-end than CPU PDLP; no benefit is established. These runs use existing binary SHA-256 `93c96173cbb918c20658d238012bb0abb8c4f6b492e1c02a540e0a44a3037a5a`, not a rebuild of the current worktree; the host driver works, while the sandbox hides the device and the temporary CUDA toolkit is absent. | `gpu/`, `evidence/gpu_hardware_host_access_check_20260928.json`, `evidence/gpu_hardware_host_access_check_reverse_20260928.json` |
| Convex QP (OSQP ADMM) | Implemented | `src/qp/admm_solver.cpp`, `src/qp/kkt.cpp`, `qp_test.cpp` |
| Sparse LDLᵀ KKT Factorization | Implemented | `src/qp/kkt.cpp`, Davis (2005) Algorithm 849 |
| Convexity Verification | Implemented | `src/qp/model.cpp`, LDLᵀ diagonal pivot check |
| Independent QP Verifier | Implemented | `src/qp/verifier.cpp`, KKT residuals & gap |
| MIQP Branch-and-Cut | Implemented | `src/milp/node_lp.cpp`, `src/milp/heuristics.cpp` |
| MILP relaxation capacity | Sparse revised simplex handles ordinary node relaxations; oversized nodes use bounded PDLP and require a finite dual bound. The five former domain failures now pass, including supply_chain_large in about 10 seconds in the recorded Release run. Full deadline and allocation coverage remains open (IR-20/21). | `src/milp/node_lp.cpp`, `evidence/readiness-domain-results.json` |
| MPS QUADOBJ & QMATRIX | Implemented | `src/io/mps.cpp`, `tests/mps_parser_test.cpp` |
| Problem classifier (W6) | Implemented | `src/model/classifier.cpp`, locked decision tree, `tests/classifier_test.cpp`, `docs/engine_selection.md` |
| GPU-assisted QP path (W3) | >100k-NNZ path, GPU-vs-CPU objective agreement, KKT verification, and device matrix-vector test passed on RTX 2050. Only residual P·x is on GPU; KKT factorization and ADMM x-update remain CPU-side, so the planned GPU x-update is incomplete. | `gpu/src/admm_matvec.cpp`, `gpu/kernels/admm_step.cu`, `tests/gpu_qp_test.cpp`, `gpu/tests/admm_gpu_test.cpp`, `src/qp/admm_solver.cpp` |
| NLP via SQP + L-BFGS (W1/D-02) | Implemented | `src/nlp/` (SQP, L-BFGS, merit line search, verifier); convex NLP, KKT <= 1e-6; `tests/nlp_sqp_test.cpp` |
| MINLP via Outer Approximation (W1/D-03) | Quadratic MPS subset only; combined QUADOBJ/NLOBJ objective and Hessian scaling verified; structural convexity screen; linear MPS equality rows represented as paired inequalities and tested end-to-end; arbitrary callback equalities and callback MINLPs rejected because their convexity cannot be certified; candidate feasibility rechecked at 1e-6; maximize objective and bound converted to original sense; `Optimal` requires finite global bound and <=1e-3 gap | `src/minlp/minlp_solver.cpp`, `src/io/nlobj_parser.cpp`, `src/api/api.cpp`, `src/nlp/nlp_verifier.cpp`; `tests/minlp_basic_test.cpp` |
| MPS NLOBJ/NLCON extension (W1/D-12/D-20) | Implemented | Quadratic objective terms and named polynomial inequalities with analytic gradients; `src/io/nlobj_parser.cpp`, `docs/nlobj_format.md`, `tests/nlobj_parser_test.cpp` |
| pybind11 Python bindings (W7/D-11) | Implemented | `python/src/bindings.cpp`, `setup.py`, `python/tests/test_bindings.py` (pytest gate in CTest `python_bindings`) |
| ML-assisted branching (W2/D-04/D-05/D-18) | **In progress; acceptance open.** Solver logging emits MCONLOG3 bipartite records. An 80-instance collection attempt yielded 8 populated strong-branching logs; 31 instances timed out and most remaining runs exited without labels. Exploratory training had no meaningful disjoint validation/test split. A narrow candidate ONNX experiment on MIPLIB `cvs16r128-89` scored >200 candidates and explored 4 vs pseudo-cost 6 nodes in two matched trials, but both timed out without incumbents. Candidate weights were held under `/tmp` and are now missing; the invalid bundled runtime artifact has been withdrawn; no model is promoted. | `src/milp/ml_branching/`, `scripts/ml/`, `tests/ml_branching_test.cpp`; restore durable candidate weights, obtain robust disjoint instance data, validate the runtime artifact and score verified solve outcomes before promotion |

---

## 2. Explicit Prototype Limitations

JSON telemetry emitted by `markov-cero-solve` reports:
```json
"limitations":"Sovereign LP/MILP/QP/MIQP/NLP/MINLP (CPU/GPU) engine."
```

### W1 NLP/MINLP Limitations (per implementation plan, LOCKED scope)

The SQP engine (D-02) explicitly does **not** handle:

- **Non-convex QP subproblems** — the L-BFGS model keeps subproblems strictly convex, so
  only a stationary point (not a global optimum) is guaranteed for non-convex NLP
  (e.g. Rosenbrock converges to (1,1) from standard starts; no global-optimality claim).
- **Problems without Lipschitz-continuous gradients** — the line search and L-BFGS update
  assume gradient consistency; discontinuous gradients may fail the merit acceptance and
  terminate as `numerical_failure` after the reset budget.
- **Infeasibility certificates** — SQP declares infeasibility only after exhausting the
  iteration budget; no Farkas certificate is produced for NLP constraints.

MINLP (D-03) is **convex-only**. The implemented MINLP path now screens the Hessians
of structurally represented quadratic objectives and `NLCON` rows before adding OA cuts.
Linear MPS equality rows are supported through equivalent paired affine inequalities.
Callback-only nonlinear MINLP and callback equalities are rejected because their global
convexity/equality structure cannot be certified. Non-convex MINLP remains post-plan scope.
NLOBJ/NLCON (Path B) express polynomial terms up to degree 2; transcendental functions require
the callback API (Path A) — see `docs/nlobj_format.md`.

### Problem Statement Capability Status (LP / MILP / QP / GPU)

- **Continuous LP**: **Implemented** (Phase 1–2).
- **MILP Branch-and-Cut**: **Implemented** (Phase 3–4).
- **GPU Acceleration**: **Implemented; net benefit not demonstrated** (Phase 5; see [docs/gpu.md](docs/gpu.md)).
  Physical RTX 2050 PDLP ran and verified all four planned scales; it was 2.60–8.64× slower
  end-to-end than CPU PDLP on these instances.
- **Convex QP & MIQP**: **Implemented** (Phase 6).
  OSQP ADMM operator splitting, quasi-definite KKT factorization, independent KKT verification.

### R1–R20 Coverage Matrix (PS-GAP closure register)

Status against `docs/sih26119_problem_statement.md` requirements. Evidence references are
clickable artifacts; "PARTIAL" rows name the exact missing piece.

| Req | Requirement (abbrev) | Status | Evidence / missing piece |
|---|---|---|---|
| R1 | Solver core, not modeling environment | **MET** | sovereign core + CLI; no modeling-env claims |
| R2 | LP, MILP, QP initial scope | **MET** | all three engines + independent verifiers |
| R3 | Extensible to MIQP/NLP/MINLP | **MET (exceeded)** | MIQP shipped; modular engine dispatch (`api.h`) |
| R4 | Revised simplex **and interior-point** | **MET** | both simplex engines **and** a Mehrotra predictor-corrector IPM (`--engine ipm`, `src/lp/interior/ipm.cpp`) with crossover to a certified vertex basis (`tests/ipm_test.cpp`); Netlib sweep: 16/17 optimal+verified, 8 crossover-certified / 8 honest simplex fallback |
| R5 | B&B, B&C, cuts, presolve, heuristics, node selection | **MET** | in-tree cut loop (RW-1), presolve, pump, reliability/strong branching; `evidence/cut_effectiveness.csv` |
| R6 | Sparse techniques + efficient numerical LA | **MET** | sparse LU + eta, sparse-first canonicalization, LDLᵀ, RW-8 refinement |
| R7 | Multi-core parallelization | **MET** | RW-2: 3.68× @ 4 threads (flugpl), TSan-clean; `evidence/benchmarks/rw2_parallel_scaling.md` |
| R8 | GPU where measurable benefits | **PARTIAL** | Three physical RTX 2050 runs verified the four scales; GPU lost end-to-end on every run. Two added order-reversed runs are pinned to binary SHA-256 `93c96173cbb918c20658d238012bb0abb8c4f6b492e1c02a540e0a44a3037a5a`; this older binary was not rebuilt from current source. No benefit is demonstrated. (`evidence/benchmarks/gpu_host_access_check_20260928.csv`, `..._reverse_20260928.csv`) |
| R9 | Numerical stability, scalability, convergence | **PARTIAL** | Harris, Ruiz and refinement are implemented; certificate tolerance consistency and the disabled `scale_200` numerical regression remain open. |
| R10 | No existing solver library | **PARTIAL — source trace review open** | `sovereignty_guard` and empty `third_party/` establish dependency isolation; provenance records disclose 2026-09-25 peer-source inspection, and clean-room status is unverified pending independent trace review |
| R11 | Industrial scope (refinery, blending, …) | **PARTIAL** | Industrial-themed cases and a synthetic refinery qualification example exist; the configured refinery model still requires feed/quality balance validation and an engineer-owned pilot before operational use. |
| R12 | Scale: thousands→millions, sparse | **PARTIAL** | A 50,000-variable, 149,600-nonzero PDLP case is documented, while simplex and MILP node relaxations still have dense limits; million-scale general solve capability is not established. |
| R13 | Robustness: degeneracy, ill-conditioning, hard MI | **PARTIAL** | Pricing, Harris, Bland fallback and a robustness dossier exist; the disabled `scale_200` regression, hard-instance failures and current MPS domain mismatch leave broad robustness unproven. |
| R14 | API or CLI sufficient | **MET for CLI/C++ API** | CLI and C++ API (`src/api/`, `include/markov_cero/api/solve.hpp`) exist; no versioned C ABI is provided. |
| R15 | MIPLIB / Netlib / Mittelmann benchmarks | **PARTIAL** | Final solver SHA-256 `beaed56a7534f8c8482819c1ad78aae69951c7b7ea98ef54d188bc43258e7cd1` completed all 255 checked-in local Netlib/MIPLIB/Mittelmann/QPLIB rows at a solver-side 15-second cap: verified optimal counts 51/98, 4/119, 4/20, 4/18; MIPLIB had four additional verified infeasibility certificates. Three rows exceeded the parent watchdog. Full upstream datasets and plan-level 300-second runs remain open. Results: `evidence/benchmarks/current_full_15s_20260928/` and `evidence/benchmarks/current_qplib_filllimit_20260928/`. |
| R16 | Compare vs ≥1 established solver | **PARTIAL** | The current 23-case, 15-second, five-solver W9 run (`evidence/comparison/current_glpk_pinned_20260928/full_comparison_report.md`) has 115 rows: markov-cero 17/23 optimal, HiGHS 17/23, GLPK 14/23, CBC 13/23, SCIP 18/23; no pair of optimal objectives disagreed. A pinned-binary, two-repeat HiGHS comparison on 20 preregistered Netlib/MIPLIB cases passed verification/objective agreement 20/20 at both one and four markov-cero threads (`evidence/compare/current_final_threads1_20260928/`, `evidence/compare/current_final_threads4_20260928/`). Median geometric runtime ratios were 15.61x and 12.76x markov-cero/HiGHS. Timing boundaries still differ between subprocess and API calls. |
| R17 | Demonstrate numerical robustness | **MET** | `evidence/robustness_dossier.md` dossier detailing condition estimation, Moler/ill-conditioned test matrices, extended-precision iterative refinement, and KKT residual guarantees |
| R18 | Transparent, extensible, sovereign | **PARTIAL — provenance review open** | docs vault, mathematical theory compendium, modular engine dispatch and dependency isolation; source independence remains unverified |
| R19 | Datasets: MIPLIB, Netlib, Mittelmann, QPLIB + cases | **MET** | MIPLIB, Netlib, Mittelmann (`data/mittelmann/`), QPLIB (`data/qp/` via `scripts/import_qplib.py` with SHA-256 provenance), and industrial cases (`examples/cases/`) |
| R20 | Optimal/near-optimal at industrial scale, faster than weaker impls | **UNPROVEN** | Large-scale capability and a 3.68× result on one flugpl parallel case are evidenced, but the broader 23-case W9 report has six markov-cero failures/timeouts. The newer repeated 20-case comparison agrees on every verified optimum yet its measured runtime ratios favor HiGHS. Broad industrial optimality and solver competitiveness require representative larger suites, stronger solve coverage, and aligned timing boundaries. |

**Current evidence register**: status claims above are individually qualified. R8 remains PARTIAL (GPU correctness tested on RTX 2050, but no speed benefit on four measured cases across three runs); R15 remains PARTIAL (all checked-in local benchmark inputs were run at 15 seconds, while upstream data and plan-level caps remain open); R16 is PARTIAL (comparison exists, methodology/timing fairness and competitiveness remain open); R20 is UNPROVEN. The earlier 19-MET summary below this register is superseded by the current audit and must not be used as a release claim.

### Architecture Problems (AP-1–AP-12) — closure register

Findings of `docs/audit/07-current-architecture.md` §C.4, with their disposition.

| # | Problem | Disposition |
|---|---|---|
| AP-1 | No interior-point engine and no crossover | **Closed** — `--engine ipm` (Mehrotra predictor-corrector, normal equations with diagonal-perturbation fallback) + rank-revealing crossover handing a certified basis to the dual simplex; `tests/ipm_test.cpp` guards the basis-warm-start contract |
| AP-2 | Evaluation subsystem missing | **Closed** — `scripts/run_compare.py` vs HiGHS 1.15.1 (R16), `data/compare/*.txt`, `compare_harness` CTest |
| AP-3 | Cuts root-only | **Closed (RW-1)** — in-tree cut loop with separation frequency, pool reuse, per-round budget |
| AP-4 | Parallel search without work stealing | **Closed (RW-2)** — load-balanced queue; 3.68× @ 4 threads (was 0.56×) |
| AP-5 | GPU loses yet is claimed; status bug in `run_gpu.py` | **Closed (RW-9)** — status-gate fixed, claim surface rescoped, `evidence/hardware.md` added |
| AP-6 | Monolithic app orchestration | **Closed (RW-4)** — orchestration in `src/api/` (`solve_file`/`solve_model`) + install target; CLI is thin |
| AP-7 | Two canonicalization paths (dense gate) | **Closed (RW-5)** — single sparse-first path; dense only as verification oracle; 512×6144 verified |
| AP-8 | No numerical robustness tooling | **Closed (RW-8)** — selective iterative refinement + condition estimate on the sparse basis |
| AP-9 | Presolve only 3 rule classes | **Closed** — 7 rule classes (empty row/col, row singleton, forcing rows, duplicate rows, dominated duplicate columns, fixed variables) with dual-feasibility-restoring postsolve; the forcing-row dual is the minimum admissible multiplier over incident columns |
| AP-10 | GPU path untested in CI | **Closed** — CUDA build job + fixed `gpu_benchmarks`/`gpu_profiling` status gate |
| AP-11 | `docs/history.md` drift | **Closed** — archived; CHANGELOG is the living record |
| AP-12 | Dead/duplicate artifacts | **Closed** — dead options and duplicate runners removed, fuzzer wired under `MARKOV_CERO_BUILD_FUZZER`, scratch dirs gone |

### Deferred Capabilities

1. **Machine Learning-Assisted Branching**:
   - **Status**: **In scope; acceptance remains open.** A candidate standard-ONNX scorer activates on MIPLIB `cvs16r128-89` and explored 4 vs 6 nodes in two matched capped runs, but both runs hit the time limit without an incumbent. The invalid bundled model artifact has been withdrawn, and training/evaluation data remain too small for promotion.
   - **Roadmap**: Current SIH implementation plan W2; do not use the superseded ED-009 deferral note as current scope.
   - **Strategy**: Explicit `--branching ml_gnn` opt-in, self-collected strong-branching labels, and the >200 candidate activation gate. See `evidence/ml_models/cvs16r128-89_node_gate.json` and F-01/F-25 in the current audit.

2. **Dual Steepest-Edge Pricing Recurrence**:
   - **Status**: **Implemented** via exact $O(m)$ Forrest–Goldfarb recurrence in `src/lp/dual/dual_simplex.cpp` and `include/markov_cero/lp/dual/dual_simplex.hpp`.
   - **Evidence**: Verified in CTest #10 `dual_simplex`.

---

## 3. Roadmaps for Completed and Deferred Capabilities

### Phase 5: GPU PDLP prototype (QP update incomplete; speed benefit unproven)
- Detailed roadmap and design: [docs/gpu.md](docs/gpu.md).
- Native C++20 matrix-free PDLP engine (`src/lp/first_order/pdlp.cpp`) serves as the CPU on-ramp.
- Implemented target: CUDA SpMV (`A * x`, `A^T * y`), vector axpy, and dot-product reductions.
- RTX 2050: four generated GPU PDLP cases passed verification in three runs; GPU was slower end-to-end in all twelve case-runs. No crossover threshold or speed benefit is established. Two added runs used an existing CUDA build, pinned by hash, that was not rebuilt from the current worktree.
- Remaining: the ADMM QP x-update linear solve is CPU-side; the planned GPU QP path is incomplete and must not be described as GPU-accelerated.
- Primal revised simplex remains CPU-bound due to serial sparse basis updates.

### Phase 6: Convex QP & MIQP (Completed)
- Target: Convex quadratic programs ($\min \frac{1}{2} x^T P x + q^T x$ with $l \le A x \le u$).
- Parser: Support for `QUADOBJ` and `QMATRIX` in MPS format (`src/io/mps.cpp`).
- Linear Algebra: Timothy Davis sparse LDLᵀ decomposition for symmetric quasi-definite KKT.
- Algorithm: ADMM operator splitting with over-relaxation and adaptive penalty parameter updates.
- Verification: Independent KKT certificate verifier checking primal/dual residuals.
- MIQP: Continuous QP relaxations at branch-and-cut tree nodes with quadratic energy heuristic.

### Phase 7: Machine Learning-Assisted Branching (Roadmap)
- Target: Fast variable selection approximating strong branching scores without LP resolves.
- Features: Variable fractionality distance, row density, objective coefficients, pseudo-costs.
- Architecture: Zero-dependency embedded ranker filtering top-$k$ candidates for strong branching.

---

## 4. MPS Parser Dialect & Boundaries

- **Supported Sections**: `NAME`, `ROWS`, `COLUMNS`, `RHS`, `RANGES`, `BOUNDS`, and `ENDATA`.
- **Integer Markers**: Binary and general integers via `'INTORG'`, `'INTEND'`, `BV`, `UI`, `LI`.
- **Security Boundaries**: Hostile input safeguards with bounded memory allocation, checked
  arithmetic dimensions, and RFC 8259-compliant JSON telemetry.

---

## 5. CLI Interface (`markov-cero-solve --help`)

The CLI interface matches `markov-cero-solve --help` exactly:

```
usage: markov-cero-solve MODEL.mps [options]
options:
  --output result.json     Write output JSON to file
  --engine primal|dual|ipm|pdlp|milp|parallel|qp|miqp|auto Select solver engine (default: auto)
  --threads N              Worker threads for parallel tree search (default: 4)
  --branching RULE         Branching rule: most_fractional|pseudo_cost|
                           strong_branching|reliability (default: pseudo_cost)
  --iteration-limit N      Maximum simplex iterations
  --max-nodes N            Maximum branch-and-cut search nodes (default: 50000)
  --time-limit SEC         Maximum search time limit in seconds (default: 60.0)
  --cuts, --no-cuts        Enable or disable Gomory & MIR mixed-integer cuts (default: enabled)
  --heuristics, --no-heuristics Enable or disable primal heuristics (default: enabled)
  --warm-start FILE        Load warm-start basis from file (dual engine)
  --save-basis FILE        Save optimal basis to file
  --presolve, --no-presolve Enable or disable presolve reductions (default: enabled)
  --scale, --no-scale       Enable or disable Ruiz matrix scaling (default: enabled)
  --max-presolve-passes N   Maximum presolve passes (default: 5)
  --ruiz-iterations N       Maximum Ruiz equilibration iterations (default: 10)
  --tolerance TOL          Relative KKT tolerance for PDLP (default: 1e-4)
  --backend cpu|gpu        PDLP execution backend (default: cpu)
  --help, -h               Show this help
```

---

## 6. Exit Codes

`markov-cero-solve` returns deterministic exit codes:

| Code | Status | Meaning |
|---|---|---|
| `0` | Optimal | Verified optimal solution found |
| `1` | Infeasible | Certified infeasible by Farkas certificate / dual ray |
| `2` | Unbounded | Certified unbounded by primal ray |
| `3` | InvalidModel | Parse error, duplicate names, or malformed MPS |
| `4` | InvalidOptions | Invalid command-line arguments or parameters |
| `5` | ResourceLimit | Node limit or time limit exceeded |
| `6` | IterationLimit | Iteration limit reached without optimality |
| `7` | NumericalFailure | Singular basis, numerical drift, or precision loss |
| `8` | Usage / I/O | File not found, unreadable path, or help requested |
