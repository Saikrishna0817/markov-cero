# Original User Request

> **Historical scope record.** This is the original 2026-09-26 project request,
> retained for traceability with a contextual preface and current path names.
> Its requested milestones and historical file references
> are not proof of completion and some cited audit files now live only in Git
> history. Use the [current status](STATUS.md) and
> [evidence index](../../evidence/INDEX.md) for present capability and results.

## 2026-09-26T19:13:35Z

# Teamwork Project Prompt — Implementation & Verification

Requested team: Full multi-agent build team with ML best practices

Implement and verify the authoritative, decision-locked 9-workstream roadmap for the `markov-cero` (SIH26119) clean-room C++20 mathematical optimization solver core, spanning numerical hardening, GPU acceleration targeting, NLP/MINLP, problem classification, Python bindings, ML branching with strict ML best practices, and comprehensive benchmark comparison.

Working directory: `/home/saikrishna/markov-initial-build`
Integrity mode: demo

## Reference Material
- Implementation Plan: `docs/audit/implementation_plan.md` (and artifact `implementation_plan.md`)
- Ground Truth & Gap Analysis: `docs/audit/00-ground-truth.md`
- Traceability Matrix: `docs/audit/21-traceability.md`
- Post-SIH Architecture: `docs/audit/22-post-sih-architecture.md`
- Robustness Dossier: `evidence/robustness_dossier.md`
- ML Best Practices: Strict featurization ordering, fixed train/val/test splits, regression/ranking metrics, confusion matrix & latency profiling

---

## Requirements

### R1. Milestone 1 — Numerical Accuracy Hardening (W5)
- Replace dense LU in the interior-point barrier solver (`src/lp/interior/ipm.cpp`) with a sparse normal equations factorizer using `SparseLU` (`src/linalg/sparse_basis.cpp`), scaling IPM from $m \approx 200$ to $m \ge 50,000$.
- Implement stagnation detection (window = 1000, threshold = 0.999) in PDLP (`src/lp/first_order/pdlp.cpp`) that triggers a basis extraction and crossover warm-start into dual simplex.
- Upgrade ADMM QP solver (`src/qp/admm_solver.cpp`) with Boyd et al. (2011) adaptive penalty parameter $\rho \in [10^{-6}, 10^6]$ and refactorization counter.
- Enforce always-on iterative refinement in `src/linalg/sparse_basis.cpp` with extended precision (`long double`) residual calculation.
- Eliminate silent failures across all engines: emit a structured `NumericalDiagnostic` in `SolveResult` with primal/dual residuals, condition estimate, failure site, and suggested recovery flag.

### R2. Milestone 2 — Problem Classification & GPU Polish (W6 + W3)
- Build sovereign model classifier (`src/model/classifier.cpp`, `include/markov_cero/model/classifier.hpp`) determining problem class (`LP`, `MILP`, `QP`, `MIQP`, `NLP`, `MINLP`) from structural properties, MPS sections, and callback presence.
- Broaden CUDA architecture support in `CMakeLists.txt` to `"all-major"` (sm_50 through sm_90) with runtime GPU compute capability verification in `gpu/src/device.cpp`.
- Develop GPU-accelerated ADMM step kernel (`gpu/kernels/admm_step.cu`, `gpu/src/admm_matvec.cpp`) for large QP instances ($NNZ(P) > 100,000$).
- Generate synthetic scale instances up to 5M nonzeros and run the empirical crossover benchmark, documenting the crossover point in `evidence/benchmarks/crossover_study.csv`.

### R3. Milestone 3 — Nonlinear & Mixed-Integer Nonlinear Programming (W1)
- Construct SQP solver (`src/nlp/sqp_solver.cpp`) with L-BFGS-B quasi-Newton Hessian approximation, Armijo/Wolfe line search on $\ell_1$ merit function, and KKT residual verifier ($\epsilon_{opt} \le 10^{-6}$).
- Implement convex MINLP Outer Approximation (`src/minlp/minlp_solver.cpp`, `src/minlp/outer_approx.cpp`) utilizing the SQP subproblem solver and master MILP branch-and-cut.
- Implement dual input modalities: programmatic C++ callback API (`NlpModel`) and custom MPS `NLOBJ` polynomial section parser (`src/io/nlobj_parser.cpp`).

### R4. Milestone 4 — Sovereign Python Bindings (W7)
- Author zero-runtime-dependency `pybind11` CPython extension in `python/_core/` exposing `markov_cero.solve()`, `markov_cero.Model`, `markov_cero.NlpModel`, and `markov_cero.SolveOptions`.
- Integrate zero-copy NumPy buffer protocol for solution vectors and sparse constraint matrices.
- Configure `pyproject.toml` and CMake targets for `pip install .` workflows.

### R5. Milestone 5 — ML-Assisted Branching with ML Best Practices (W2)
- Instrument strong branching data logger in `src/milp/ml_branching/training_logger.cpp` to record bipartite graph features and exact branch improvement scores on MIPLIB 2017 easy instances.
- Apply strict ML best practices: partition dataset chronologically/by-instance into fixed train (70%), validation (15%), and test (15%) splits before fitting normalizers; handle outliers and evaluate ranking metrics (Kendall's $\tau$, NDCG@k) alongside mean squared error.
- Train a 2-layer bipartite GCN (~20k params), export to quantized Int8 ONNX (`data/ml_models/branching_scorer.onnx`), and implement zero-dependency C++ inference (`src/milp/ml_branching/onnx_scorer.cpp`) active via `--branching ml_gnn`.

### R6. Milestone 6 — Full Datasets & Comprehensive Solver Comparison (W8 + W9)
- Automate download and verification scripts for full Netlib LP (97 instances), MIPLIB 2017 benchmark subset, Mittelmann suites, and convex QPLIB instances with SHA-256 provenance JSONs.
- Author automated comparative harness (`scripts/run_full_compare.py`) benchmarking markov-cero against local free solvers (HiGHS, GLPK, COIN-OR CBC, SCIP) and Mittelmann reference tables for commercial solvers (CPLEX, Gurobi, FICO Xpress).
- Generate Dolan-Moré performance profiles (`dolan_more_lp.svg`, `dolan_more_milp.svg`) and aggregate reports in `evidence/comparison/`.

---

## Acceptance Criteria

### Correctness & Numerical Quality
- [ ] 100% pass rate on all existing 44 CTest targets with zero regressions.
- [ ] Sparse IPM successfully solves Netlib instances exceeding 200 rows (e.g. `sc205`, `share1b`) without memory explosion or singular factorization aborts.
- [ ] PDLP crossover successfully resolves 100% of previously stalling Netlib test instances (`kb2`, `lotfi`, `beaconfd`) to certified KKT $\le 10^{-7}$.
- [ ] Every non-optimal or failed solve emits a structured `NumericalDiagnostic` containing valid residuals and guidance.

### Functionality & Coverage
- [ ] SQP converges to certified KKT $\le 10^{-6}$ on convex NLP benchmarks and reaches the known minimum on Rosenbrock within $10^{-3}$.
- [ ] MINLP outer approximation successfully solves convex mixed-integer nonlinear problems to integer feasibility and optimality.
- [ ] Problem classifier correctly assigns `LP`, `MILP`, `QP`, `MIQP`, `NLP`, `MINLP` across unit test suites.
- [ ] Python bindings compile cleanly, allow constructing/solving models from Python, and pass all `pytest` suites.
- [ ] ML branching achieves equal or fewer branch-and-bound nodes than pseudo-cost branching on `stein15.mps`.

### Benchmarking & Comparison
- [ ] Full Netlib, MIPLIB easy, Mittelmann, and QPLIB runs produce validated CSV records in `evidence/`.
- [ ] Comparative benchmark harness runs against HiGHS, GLPK, CBC, and SCIP, generating publication-grade Dolan-Moré performance profile curves.
- [ ] Zero third-party solver libraries linked; complete clean-room C++20 sovereignty preserved.
