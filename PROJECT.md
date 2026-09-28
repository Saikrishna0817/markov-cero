# Project: markov-cero (SIH26119) Clean-Room C++20 Mathematical Optimization Solver

## Architecture
Clean-room sovereign C++20 mathematical optimization solver core supporting LP, MILP, QP, MIQP, NLP, and MINLP.
- **LP Engines**: Primal Simplex, Dual Simplex, Sparse Normal Equations IPM (Interior Point Method), First-Order PDLP (Primal-Dual Hybrid Gradient) with dual simplex crossover.
- **QP Engines**: Active Set QP, ADMM QP with adaptive penalty parameter $\rho$, KKT refactorization, and GPU step acceleration.
- **NLP / MINLP Engines**: Sovereign SQP with L-BFGS-B Hessian approximation and $\ell_1$ merit line search; Outer Approximation convex MINLP master solver.
- **Branch & Bound / MILP**: Sovereign branch-and-cut with strong branching and pseudo-cost; an opt-in ML scorer exists, with training and genuine ML-on acceptance still in progress.
- **Problem Classification & Model API**: Sovereign classifier determining problem type and routing to optimal engine; structured `NumericalDiagnostic` for all solves.
- **Bindings**: Sovereign zero-runtime-dependency `pybind11` CPython extension with zero-copy NumPy buffer protocol.
- **Benchmarking**: Automated comparative harness benchmarking markov-cero against HiGHS, GLPK, CBC, SCIP, and commercial Mittelmann reference tables.

## Feature Inventory
Every feature identified during survey mapped to its designated milestone:

| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | SparseLU IPM Normal Equations | Replace dense $m \times m$ LU in `ipm.cpp` with sparse $A D A^T$ factorization via `SparseLu` | M1 | ORIGINAL_REQUEST §R1, D-14 |
| 2 | PDLP Stagnation Detection | Windowed stagnation tracking (window=1000, threshold=0.999) in `pdlp.cpp` | M1 | ORIGINAL_REQUEST §R1, D-15 |
| 3 | PDLP Dual Simplex Crossover | Extract candidate basis from complementary slackness and warm-start dual simplex | M1 | ORIGINAL_REQUEST §R1, D-15 |
| 4 | ADMM Adaptive Penalty $\rho$ | Update $\rho \in [10^{-6}, 10^6]$ adaptively following Boyd et al. (2011) | M1 | ORIGINAL_REQUEST §R1, D-16 |
| 5 | ADMM KKT Refactorization Counter | Track refactorizations in `QpSolution` triggered by adaptive $\rho$ updates | M1 | ORIGINAL_REQUEST §R1, D-16 |
| 6 | Always-On Iterative Refinement | Always execute extended-precision `long double` refinement pass with $10^{-14}$ early exit | M1 | ORIGINAL_REQUEST §R1, D-17 |
| 7 | Structured NumericalDiagnostic | Emit residuals, condition number, failure site, and suggested recovery in `SolveResult` | M1 | ORIGINAL_REQUEST §R1, C-3 |
| 8 | Sovereign Problem Classifier | Identify LP, MILP, QP, MIQP, NLP, MINLP from structural properties and callbacks | M2 | ORIGINAL_REQUEST §R2, D-02 |
| 9 | Classifier Engine Selection Rules | Auto-route problem classes based on dimensions and density thresholds | M2 | ORIGINAL_REQUEST §R2, D-02 |
| 10 | CUDA Architecture Broadening | Support `"all-major"` (sm_50 through sm_90) in `CMakeLists.txt` | M2 | ORIGINAL_REQUEST §R2, D-07 |
| 11 | Runtime GPU Capability Guard | Verify `props.major >= 5` in `gpu/src/device.cpp` before allocating GPU buffers | M2 | ORIGINAL_REQUEST §R2, D-07 |
| 12 | GPU ADMM Step Kernel | Accelerate $(P + \rho I) x$ in `gpu/kernels/admm_step.cu` and `gpu/src/admm_matvec.cpp` | M2 | ORIGINAL_REQUEST §R2, D-08 |
| 13 | Empirical Crossover Benchmark | Scale instances up to 5M nonzeros; generate `evidence/benchmarks/crossover_study.csv` | M2 | ORIGINAL_REQUEST §R2, D-08 |
| 14 | Sovereign SQP Solver Core | Construct sequential quadratic programming solver in `src/nlp/sqp_solver.cpp` | M3 | ORIGINAL_REQUEST §R3, D-01 |
| 15 | L-BFGS-B Hessian Approximation | Maintain compact two-loop quasi-Newton Hessian approximation ($m=10$) | M3 | ORIGINAL_REQUEST §R3, D-01 |
| 16 | Armijo-Wolfe Line Search | Backtracking line search with $\ell_1$ merit function and descent validation | M3 | ORIGINAL_REQUEST §R3, D-01 |
| 17 | KKT Residual Verifier | Verify optimality conditions: stationarity, feasibility, complementarity $\le 10^{-6}$ | M3 | ORIGINAL_REQUEST §R3, D-01 |
| 18 | Convex MINLP Outer Approximation | Implement Outer Approximation in `src/minlp/minlp_solver.cpp` & `outer_approx.cpp` | M3 | ORIGINAL_REQUEST §R3, D-03 |
| 19 | Programmatic NlpModel API | C++ callback interface with user-defined objective, gradient, constraints, and Jacobian | M3 | ORIGINAL_REQUEST §R3, D-04 |
| 20 | MPS NLOBJ Section Parser | Parse polynomial non-linear objective sections from extended MPS files | M3 | ORIGINAL_REQUEST §R3, D-04 |
| 21 | Sovereign pybind11 CPython Module | Build `python/_core/` CPython extension exposing solver API with zero runtime deps | M4 | ORIGINAL_REQUEST §R4, D-05 |
| 22 | Python Model / NlpModel Bindings | Python classes `markov_cero.Model`, `NlpModel`, `SolveOptions`, and `solve()` | M4 | ORIGINAL_REQUEST §R4, D-05 |
| 23 | Zero-Copy NumPy Buffer Protocol | Direct buffer mapping for solution vectors, constraint matrices, and Jacobians | M4 | ORIGINAL_REQUEST §R4, D-06 |
| 24 | Pip Build Integration | Configure `pyproject.toml` and CMake targets for seamless `pip install .` | M4 | ORIGINAL_REQUEST §R4, D-05 |
| 25 | Strong Branching Data Logger | Record bipartite graph features and branch scores on MIPLIB easy instances | M5 | ORIGINAL_REQUEST §R5, D-10 |
| 26 | Strict ML Data Partitioning | Fixed instance-based 70% train, 15% validation, 15% test splits | M5 | ORIGINAL_REQUEST §R5, D-10 |
| 27 | Bipartite Graph Featurization | Strict feature vector ordering for constraint, variable, and edge representations | M5 | ORIGINAL_REQUEST §R5, D-10 |
| 28 | ML Ranking & Regression Metrics | Evaluate Kendall's $\tau$, NDCG@k, and MSE during training | M5 | ORIGINAL_REQUEST §R5, D-10 |
| 29 | 2-Layer Bipartite GCN Model | Train ~20k parameter bipartite GCN in PyTorch | M5 | ORIGINAL_REQUEST §R5, D-11 |
| 30 | Quantized Int8 ONNX Export | Export model to `data/ml_models/branching_scorer.onnx` with calibration | M5 | ORIGINAL_REQUEST §R5, D-11 |
| 31 | Zero-Dependency C++ Inference | Sovereign pure C++20 Int8 GCN evaluator in `src/milp/ml_branching/onnx_scorer.cpp` | M5 | ORIGINAL_REQUEST §R5, D-12 |
| 32 | CLI ML Branching Strategy | Expose `--branching ml_gnn` achieving $\le 157$ nodes on `stein15.mps` | M5 | ORIGINAL_REQUEST §R5, D-12 |
| 33 | Netlib LP Full Suite Ingestion | Ingest all 97 standard Netlib LP instances with SHA-256 provenance JSONs | M6 | ORIGINAL_REQUEST §R6, D-18 |
| 34 | MIPLIB 2017 Easy Suite Ingestion | Ingest designated MIPLIB 2017 easy benchmark instances with SHA-256 provenance | M6 | ORIGINAL_REQUEST §R6, D-18 |
| 35 | Mittelmann Suite Ingestion | Ingest standard Mittelmann LP and MILP instances with provenance | M6 | ORIGINAL_REQUEST §R6, D-18 |
| 36 | Convex QPLIB Ingestion | Ingest standard convex QPLIB instances with provenance JSONs | M6 | ORIGINAL_REQUEST §R6, D-18 |
| 37 | Automated Multi-Solver Comparison | Author `scripts/run_full_compare.py` against HiGHS, GLPK, CBC, SCIP, & Mittelmann tables | M6 | ORIGINAL_REQUEST §R6, D-19 |
| 38 | Dolan-Moré Performance Profiles | Generate `dolan_more_lp.svg` and `dolan_more_milp.svg` in `evidence/comparison/` | M6 | ORIGINAL_REQUEST §R6, D-20 |

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | Numerical Accuracy Hardening | SparseLU IPM, PDLP stagnation & dual crossover, ADMM adaptive $\rho$, always-on iterative refinement, structured `NumericalDiagnostic` | None | IN_PROGRESS |
| 2 | Problem Classification & GPU Polish | Sovereign classifier, CMake CUDA all-major, sm_50 runtime guard, GPU ADMM step kernel, empirical crossover study | M1 | PLANNED |
| 3 | Nonlinear & MINLP | Sovereign SQP solver with L-BFGS-B, Armijo-Wolfe line search, KKT verifier, Outer Approximation convex MINLP, `NlpModel` API, `NLOBJ` parser | M1, M2 | PLANNED |
| 4 | Sovereign Python Bindings | `pybind11` CPython extension in `python/_core/`, zero-copy NumPy buffer protocol, `pyproject.toml`, pytest suite | M1, M2, M3 | PLANNED |
| 5 | ML-Assisted Branching with ML Best Practices | Strong branching training logger, fixed 70/15/15 splits, bipartite GCN training, Int8 ONNX export, zero-dependency C++ inference, `--branching ml_gnn` | M1, M4 | IN PROGRESS; acceptance gate open |
| 6 | Full Datasets & Comparative Benchmark Harness | Full Netlib 97, MIPLIB 2017 easy, Mittelmann, QPLIB downloaders & SHA-256 provenance; `scripts/run_full_compare.py` vs HiGHS, GLPK, CBC, SCIP + Mittelmann commercial tables; Dolan-Moré SVGs | M1, M2, M3, M5 | PLANNED |
| E2E | E2E Testing Track | Requirement-driven opaque-box test suites (Tiers 1-4) across all 38 inventoried features; publishing `TEST_READY.md` | Runs in parallel | IN_PROGRESS |
| Final | 100% E2E Pass & Adversarial Hardening | Phase 1: 100% pass across all Tier 1-4 tests; Phase 2: Tier 5 adversarial coverage hardening | M1-M6, E2E | PLANNED |

## Interface Contracts

### 1. Sparse Basis & IPM (`src/linalg/sparse_basis.cpp` ↔ `src/lp/interior/ipm.cpp`)
- `linalg::SparseLu::factorize(const SparseCsc& matrix, const SparseLuOptions& options)` -> `SparseLuFactorization`
- `SparseLuFactorization::solve(const std::vector<double>& rhs)` -> `std::vector<double>`
- Always-on iterative refinement with `long double` residuals and early exit at $\|r\|_\infty < 10^{-14}$.

### 2. PDLP Stagnation & Crossover (`src/lp/first_order/pdlp.cpp` ↔ `src/lp/dual/dual_simplex.cpp`)
- Stagnation check: improvement $< 0.1\%$ over window of 1000 iterations.
- Crossover basis extraction: partition variables by complementarity slackness ($x_j > \epsilon$, $s_j \le \epsilon$).
- Warm-start dual simplex with candidate basis indices.

### 3. ADMM QP Solver (`src/qp/admm_solver.cpp` ↔ `include/markov_cero/qp/admm_solver.hpp`)
- Adaptive penalty update:
  $$\rho \leftarrow \min(\rho \cdot \tau_{\text{incr}}, 10^6) \quad \text{if } \|r_{\text{prim}}\|_\infty > \mu \|r_{\text{dual}}\|_\infty$$
  $$\rho \leftarrow \max(\rho / \tau_{\text{decr}}, 10^{-6}) \quad \text{if } \|r_{\text{dual}}\|_\infty > \mu \|r_{\text{prim}}\|_\infty$$
  with $\mu = 10, \tau_{\text{incr}} = 2, \tau_{\text{decr}} = 2$.
- `QpSolution` struct includes `std::size_t refactorization_count`.

### 4. Structured Numerical Diagnostics (`include/markov_cero/api/solve.hpp`)
- `struct NumericalDiagnostic`:
  - `double primal_residual{0.0};`
  - `double dual_residual{0.0};`
  - `double condition_estimate{0.0};`
  - `std::string failure_site;`
  - `std::string suggested_recovery;`
- Embedded in `SolveResult` and serialized in CLI JSON outputs.

### 5. Sovereign Problem Classifier (`src/model/classifier.cpp`)
- `model::ProblemClass classify_problem(const Model& model, const NlpModel* nlp = nullptr)` -> `{ LP, MILP, QP, MIQP, NLP, MINLP }`
- `std::string select_engine(ProblemClass pclass, const ModelDimensions& dims, double density)`

### 6. SQP & MINLP (`src/nlp/sqp_solver.cpp` ↔ `src/minlp/minlp_solver.cpp`)
- `NlpModel`: objective callback $f(x)$, gradient $\nabla f(x)$, constraint callbacks $g_i(x)$, Jacobian $\nabla g_i(x)$.
- `SqpSolver::solve(const NlpModel& nlp, const SqpOptions& options)` -> `SqpResult` (KKT residual $\le 10^{-6}$).
- `MinlpSolver::solve(const MinlpModel& minlp, const MinlpOptions& options)` -> `MinlpResult`.

### 7. Python Bindings (`python/_core/`)
- Module `_core`: classes `Model`, `NlpModel`, `SolveOptions`, `SolveResult`, function `solve()`.
- Zero-copy buffer protocol via `pybind11::buffer_info` without unnecessary copies.

### 8. ML Branching (`src/milp/ml_branching/`)
- Pure C++20 sovereign Int8 GCN evaluator in `onnx_scorer.cpp`.
- Zero external dependencies: no `libtorch`, no `onnxruntime`.

## Code Layout
- `include/markov_cero/`: Public API headers
  - `api/`: `solve.hpp`, `options.hpp`, `diagnostics.hpp`
  - `model/`: `model.hpp`, `classifier.hpp`, `canonical.hpp`
  - `lp/`: `primal/`, `dual/`, `interior/`, `first_order/`
  - `qp/`: `active_set.hpp`, `admm_solver.hpp`
  - `nlp/`: `sqp_solver.hpp`, `nlp_model.hpp`, `lbfgs.hpp`
  - `minlp/`: `minlp_solver.hpp`, `outer_approx.hpp`
  - `milp/`: `branch_selector.hpp`, `ml_branching/`
- `src/`: Core implementation sources
- `gpu/`: CUDA kernels and device acceleration
- `python/`: `pybind11` CPython bindings and `pyproject.toml`
- `data/`: Benchmark datasets (`netlib/`, `miplib/`, `mittelmann/`, `qp/`, `ml_models/`)
- `scripts/`: Toolchain scripts (`check-sovereignty.py`, `run_full_compare.py`, etc.)
- `evidence/`: Benchmark results, comparison reports, and performance profiles
