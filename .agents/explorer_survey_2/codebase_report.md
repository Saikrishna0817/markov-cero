# Comprehensive Codebase Survey & Gap Analysis Report
**Target**: `markov-cero` (SIH26119 Clean-Room C++20 Mathematical Optimization Solver Core)  
**Surveyor**: `explorer_survey_2` (teamwork_preview_explorer)  
**Date**: 2026-09-27  
**Workspace Root**: `/home/saikrishna/markov-initial-build`  
**Authoritative Reference**: `.agents/ORIGINAL_REQUEST.md`

---

## 1. Executive Summary

`markov-cero` is an in-tree, clean-room C++20 sovereign mathematical optimization solver core implementing continuous LP (Primal Simplex, Dual Simplex, Dense IPM with crossover, First-Order PDLP), MILP Branch-and-Cut (GMI/MIR cuts, strong branching, heuristics, parallel tree search), Convex QP/MIQP (ADMM operator splitting, sparse Timothy Davis $LDL^T$ factorization, KKT verifier), and GPU acceleration (CUDA PDHG step kernels with CPU emulation fallbacks).

The codebase currently compiles under C++20 with strict compiler flags (`-Wall -Wextra -Wpedantic -Werror`), passes sovereignty verification (`scripts/check-sovereignty.py`), and executes all existing **44 core CTest targets with a 100% pass rate in 0.62 seconds**.

However, relative to the 6 authoritative milestones in `ORIGINAL_REQUEST.md`, significant architectural and algorithmic gaps exist:
1. **Milestone 1 (Numerical Hardening)**: IPM (`src/lp/interior/ipm.cpp`) relies on dense $m \times m$ normal equations and `DenseLu`, bottlenecking at $m \approx 200$; PDLP lacks windowed stagnation detection and crossover into dual simplex; ADMM QP lacks Boyd et al. (2011) $\rho \in [10^{-6}, 10^6]$ adaptation and refactorization tracking; Sparse basis iterative refinement is selective rather than always-on; and structured `NumericalDiagnostic` is missing from `SolveResult`.
2. **Milestone 2 (Classification & GPU Polish)**: Sovereign problem classifier (`src/model/classifier.cpp`) is missing; CUDA architectures in `CMakeLists.txt` exclude sm_50 to sm_70; `gpu/src/device.cpp` lacks runtime compute capability verification; GPU ADMM step kernel (`gpu/kernels/admm_step.cu`, `gpu/src/admm_matvec.cpp`) is not implemented; and synthetic scale crossover benchmarks up to 5M nonzeros are incomplete.
3. **Milestone 3 (Nonlinear & MINLP)**: `src/nlp/` and `src/minlp/` do not exist; SQP solver with L-BFGS-B, outer approximation MINLP, C++ `NlpModel` callback API, and MPS `NLOBJ` parser are missing.
4. **Milestone 4 (Python Bindings)**: `python/` directory, `pyproject.toml`, and `pybind11` CPython extension with zero-copy NumPy buffers do not exist.
5. **Milestone 5 (ML Branching)**: `src/milp/ml_branching/`, strong branching data logger, bipartite GCN Int8 ONNX inference engine, and `--branching ml_gnn` are missing.
6. **Milestone 6 (Comparison & Full Datasets)**: Automated download/verification of full Netlib (97 instances), QPLIB, automated comparative harness (`scripts/run_full_compare.py`), and Dolan-Moré performance profiles are pending completion.

---

## 2. Build System & CMake Architecture

### 2.1 Root `CMakeLists.txt`
- **Location**: `/home/saikrishna/markov-initial-build/CMakeLists.txt` (314 lines, 17,206 bytes).
- **CMake Version & Language**: `cmake_minimum_required(VERSION 3.25)`, `project(markov-cero VERSION 0.5.2 LANGUAGES CXX)`.
- **Primary Static Library Target**: `markov_cero_core` (43 C++ sources from `src/` and `gpu/src/`).
- **Compiler Requirements & Flags**:
  - `target_compile_features(markov_cero_core PUBLIC cxx_std_20)`.
  - Flags: `-Wall -Wextra -Wpedantic`.
  - Warnings as errors: `MARKOV_CERO_WARNINGS_AS_ERRORS` (default `ON`, sets `-Werror`).
  - GCC suppression: `-Wno-maybe-uninitialized`.
  - Sanitizers: `MARKOV_CERO_ENABLE_ASAN_UBSAN` (`-fsanitize=address,undefined -fno-omit-frame-pointer`) and `MARKOV_CERO_ENABLE_TSAN` (`-fsanitize=thread`). Both are mutually exclusive.
- **CUDA Configuration**:
  - Controlled by `MARKOV_CERO_ENABLE_CUDA` (default `OFF`).
  - When enabled: `enable_language(CUDA)`, standard `cxx_std_20`, define `MARKOV_CERO_HAS_CUDA=1`.
  - Current architecture configuration:
    ```cmake
    set(CMAKE_CUDA_ARCHITECTURES "75;80;86;89;90" CACHE STRING "CUDA architectures")
    ```
    *Gap*: Excludes sm_50, sm_52, sm_60, sm_61, sm_70 required for `"all-major"` (sm_50 through sm_90).
  - CUDA kernel sources added when enabled:
    - `gpu/kernels/pdhg_step.cu`
    - `gpu/kernels/reduce.cu`
    - `gpu/kernels/spmv.cu`
    - `gpu/kernels/vector_ops.cu`
- **Executables Built**:
  - `markov-cero-info` (`apps/markov_cero_info.cpp`)
  - `markov-cero-mps-inspect` (`apps/markov_cero_mps_inspect.cpp`)
  - `markov-cero-solve` (`apps/markov_cero_solve.cpp`)
  - Fuzzer targets under `MARKOV_CERO_BUILD_FUZZER`: `mps_fuzz`, `sparse_basis_fuzz`.
- **Install Target**:
  - Library `markov_cero_core` to `lib/`
  - Binaries to `bin/`
  - Headers `include/markov_cero` to `include/`
  - Metadata `LICENSE`, `NOTICE`, `PROVENANCE.md` to `share/markov-cero/`.
- **CMake Presets**: `/home/saikrishna/markov-initial-build/CMakePresets.json` defines:
  `gcc-debug`, `gcc-release`, `clang-debug`, `clang-release`, `gcc-asan-ubsan`.

### 2.2 Clean-Room Sovereignty Enforcement
- **Script**: `/home/saikrishna/markov-initial-build/scripts/check-sovereignty.py`.
- **CTest Target**: `sovereignty_guard` (CTest #35).
- **Enforced Rules**:
  - Disallows `FetchContent`, `ExternalProject_Add`, `add_subdirectory`.
  - Disallows linking against 28 forbidden libraries: `glpk`, `gurobi`, `cplex`, `xpress`, `highs`, `clp`, `cbc`, `coin`, `osqp`, `scs`, `ipopt`, `scip`, `mosek`, `eigen`, `boost`, `fmt`, `spdlog`, `gtest`, `nlohmann`, `torch`, `onnx`, `onnxruntime`, `alglib`, `ceres`.
  - Validates dynamic symbols with `nm -D` and dynamic libraries with `ldd` against whitelist: `linux-vdso.so`, `libstdc++.so`, `libm.so`, `libgcc_s.so`, `libc.so`, `ld-linux-x86-64.so`, `libpthread.so`, `libdl.so`, `librt.so`, `libcuda.so`, `libcudart.so`, sanitizers.

---

## 3. Directory Layout & Existing Headers

### 3.1 `include/markov_cero/` Directory Structure (39 Headers)
```
include/markov_cero/
├── analysis/
│   └── iis_analyzer.hpp             # Irreducible Infeasible Subsystem (Chinneck-Dravnieks)
├── api/
│   └── solve.hpp                    # SolveOptions, SolveResult, solve_file(), solve_model()
├── core/
│   └── exceptions.hpp               # Core domain exceptions
├── foundation/
│   └── build_info.hpp               # version(), milestone(), contains_solver_algorithms()
├── io/
│   ├── lp_parser.hpp                # CPLEX LP format parser
│   └── mps.hpp                      # Free-format MPS & QPS parser (parse_mps, parse_mps_string)
├── linalg/
│   ├── dense_lu.hpp                 # DenseLu with partial pivoting
│   └── sparse_basis.hpp             # SparseCsc, SparseLu, SparseBasisFactorization
├── lp/
│   ├── dual/
│   │   └── dual_simplex.hpp         # Dual simplex, BasisState, Forrest-Goldfarb steepest edge
│   ├── first_order/
│   │   └── pdlp.hpp                 # PdlpOptions, PdlpResult, solve_pdlp()
│   ├── interior/
│   │   └── ipm.hpp                  # Mehrotra predictor-corrector IPM with crossover
│   └── reference/
│       └── revised_simplex.hpp      # Primal revised simplex reference solver
├── milp/
│   ├── branch_node.hpp              # SearchNode, NodeSelection
│   ├── branch_selector.hpp          # VariablePseudoCost, BranchingStrategy
│   ├── cover.hpp                    # Knapsack cover cuts
│   ├── cut_pool.hpp                 # Cut, CutPool, cut separation loop
│   ├── cuts.hpp                     # Cut generator interfaces
│   ├── gomory.hpp                   # Gomory Mixed-Integer (GMI) cuts
│   ├── heuristics.hpp               # Feasibility pump, rounding heuristics
│   ├── milp_solver.hpp              # MilpOptions, MilpResult, solve_milp()
│   ├── mir.hpp                      # Mixed-Integer Rounding (MIR) cuts
│   ├── node_lp.hpp                  # Node LP relaxation solve
│   ├── parallel_tree_search.hpp     # ParallelOptions, solve_parallel() with C++20 jthread
│   ├── shared_incumbent.hpp         # Thread-safe incumbent tracker
│   ├── strong_branching.hpp         # evaluate_strong_branching()
│   └── work_queue.hpp               # Thread-safe load-balanced priority work queue
├── model/
│   └── model.hpp                    # Model, Bound, BoundKind, SparseMatrixCSC, VariableType
├── presolve/
│   ├── presolve.hpp                 # 7 presolve reduction classes
│   └── presolve_stack.hpp           # Postsolve restoration stack
├── qp/
│   ├── admm_solver.hpp              # AdmmQpSolver, solve_qp(), QpOptions, QpSolution
│   ├── kkt.hpp                      # Timothy Davis LDLT quasi-definite KKT solver
│   ├── model.hpp                    # QuadraticModel, SparseSymmetricMatrix
│   └── verifier.hpp                 # Independent KKT QP verifier
├── refinery/
│   ├── pooling_slp.hpp              # Haverly bilinear pooling via SLP
│   └── refinery_model.hpp           # Crude assay, product specs, refinery LP builder
├── scale/
│   └── ruiz_scaling.hpp             # Ruiz equilibration & unscaling
├── transform/
│   ├── canonicalize.hpp             # CanonicalModel (dense matrix)
│   └── sparse_canonical_model.hpp   # SparseCanonicalModel (SparseCsc matrix)
└── verify/
    ├── primal_verifier.hpp          # verify_primal()
    └── reference_lp_verifier.hpp    # verify_reference()
```

### 3.2 `gpu/` Directory Structure (24 Files)
```
gpu/
├── include/markov_cero/gpu/
│   ├── buffer.hpp                   # DeviceBuffer<T> with RAII & host fallback
│   ├── csr.hpp                      # DeviceCsr (from_csc, transpose_from_csc, to_csc_host)
│   ├── device.hpp                   # DeviceInfo, is_gpu_available(), get_device_info()
│   ├── kernels.hpp                  # Kernel launches & CPU emulation signatures
│   └── pdhg_step.hpp                # PdhgState, solve_pdlp_gpu()
├── kernels/
│   ├── pdhg_step.cu                 # PDHG primal/dual update CUDA kernel
│   ├── reduce.cu                    # Parallel reduction kernels (sum, max, inf-norm)
│   ├── spmv.cu                      # CSR SpMV CUDA kernel
│   └── vector_ops.cu                # Elementwise axpy, clamp, project kernels
├── src/
│   ├── buffer.cpp                   # Host fallback buffer allocation
│   ├── csr.cpp                      # Host-side CSR/CSC conversions
│   ├── device.cpp                   # cudaGetDeviceProperties wrapper
│   ├── pdhg_step.cpp                # solve_pdlp_gpu driver & CPU fallback
│   ├── reduce.cpp                   # Reduction CPU fallback
│   ├── spmv.cpp                     # SpMV CPU fallback
│   └── vector_ops.cpp               # Vector ops CPU fallback
└── tests/
    ├── equivalence_test.cpp         # CPU vs GPU output equivalence
    ├── gpu_buffer_test.cpp          # DeviceBuffer upload/download
    ├── pdhg_adaptive_test.cpp       # Adaptive step size test
    ├── pdhg_kkt_test.cpp            # KKT convergence test
    ├── pdhg_restart_test.cpp        # Restart policy test
    ├── pdhg_step_test.cpp           # Single-step iteration test
    ├── pdhg_timing_test.cpp         # 4-stage timing profiling
    └── reduction_test.cpp           # Reduction correctness
```

---

## 4. Deep-Dive Subsystem Audit & Gap Analysis

### 4.1 Interior-Point Solver (`src/lp/interior/ipm.cpp`)
- **Headers**: `include/markov_cero/lp/interior/ipm.hpp`
- **Implementation**: `src/lp/interior/ipm.cpp` (495 lines).
- **Public Interface**:
  ```cpp
  struct Options {
      std::size_t iteration_limit{100};
      double relative_tolerance{1e-8};
      bool enable_crossover{true};
  };
  struct Result {
      lp::reference::SolveStatus status;
      std::vector<double> primal;
      std::vector<double> dual;
      double objective{0.0};
      std::size_t iterations{0};
      std::string message;
      bool crossover_applied{false};
      std::optional<lp::dual::BasisState> basis_state;
  };
  [[nodiscard]] Result solve(const transform::CanonicalModel& model, const Options& options = {});
  ```
- **Current Algorithm**:
  1. Input is `const transform::CanonicalModel& model` containing a dense $m \times n$ matrix (`linalg::DenseMatrix`).
  2. Converts to `SparseCanonicalModel` temporarily to compute Ruiz equilibration scalers, then builds a dense $m \times n$ matrix `a_scaled`.
  3. Mehrotra Predictor-Corrector iteration:
     - Forms dense $m \times m$ normal equations matrix:
       ```cpp
       std::vector<double> d_row_major(m * m, 0.0);
       for (std::size_t i = 0; i < m; ++i)
           for (std::size_t k = 0; k < m; ++k) {
               long double sum = 0.0L;
               for (std::size_t j = 0; j < n; ++j) {
                   const double d_j = std::clamp(x[j] / s[j], 1e-12, 1e12);
                   sum += static_cast<long double>(a.values[i * n + j]) * d_j * a.values[k * n + j];
               }
               if (sum != 0.0L) d_row_major[i * m + k] = static_cast<double>(sum);
           }
       ```
     - Factorizes using `linalg::DenseLu::factorize(dense_from_rows(m, m, d_row_major))` with diagonal perturbation fallback ($10^{-10}$ to $10^{-2}$).
     - Solves affine step, calculates centering parameter $\sigma = (\mu_{aff} / \mu)^3$, solves corrector step, and updates primal/dual iterates with fraction-to-boundary rule ($\tau = 0.995$).
  4. Crossover:
     - `crossover_basis` performs dense Gaussian row reduction on candidate columns sorted by magnitude of $x_j$, constructing candidate basis of size $m$.
     - Factorizes candidate basis with `DenseLu`.
     - Passes basis to `lp::dual::solve()` as warm start to certify a true basic feasible vertex.
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R1)**:
  - **Dense LU Bottleneck**: $O(m^3)$ arithmetic operations and $O(m^2)$ memory storage limit IPM to small problems ($m \approx 200$). Netlib problems with $m > 200$ (e.g. `sc205`, `share1b`) cannot scale or risk memory explosion.
  - **Sparse Normal Equations Factorizer Required**: Must replace `DenseLu` with `SparseLU` (`src/linalg/sparse_basis.cpp`) acting on a sparse $A D A^T$ matrix representation (`SparseCsc`).
  - **Interface Update**: IPM must natively accept `transform::SparseCanonicalModel` (or `model::Model`) rather than requiring a dense `CanonicalModel`.

---

### 4.2 First-Order PDLP Solver (`src/lp/first_order/pdlp.cpp`)
- **Headers**: `include/markov_cero/lp/first_order/pdlp.hpp`
- **Implementation**: `src/lp/first_order/pdlp.cpp` (420 lines).
- **Public Interface**:
  ```cpp
  enum class Backend { cpu, gpu };
  enum class RestartStrategy { none, adaptive, fixed };
  struct PdlpOptions {
      std::size_t max_iterations{100000};
      std::size_t restart_every{40};
      double primal_tolerance{1e-4};
      double dual_tolerance{1e-4};
      double gap_tolerance{1e-4};
      double step_size_reduction{0.9};
      Backend backend{Backend::cpu};
      RestartStrategy restart_strategy{RestartStrategy::adaptive};
      double restart_reduction_factor{0.368};
      bool adaptive_step_size{true};
      bool adaptive_primal_weight{true};
      double initial_primal_weight{0.0};
      double primal_weight_smoothing{0.5};
      bool ruiz_scaling{true};
      std::size_t ruiz_iterations{10};
  };
  struct PdlpResult {
      PdlpStatus status;
      std::vector<double> primal;
      std::vector<double> dual;
      double objective{0.0};
      double primal_infeasibility{0.0};
      double dual_infeasibility{0.0};
      double duality_gap{0.0};
      double tolerance{1e-4};
      std::size_t iterations{0};
      std::string message;
      double h2d_ms{0.0}, kernel_ms{0.0}, d2h_ms{0.0}, total_ms{0.0};
  };
  [[nodiscard]] PdlpResult solve_pdlp(const model::Model& model, const PdlpOptions& options = {});
  ```
- **Current Algorithm**:
  1. Ruiz matrix equilibration of constraint matrix.
  2. Diagonal preconditioning step sizes $\tau_j = (\eta / \omega) / \|A_{:j}\|_1$ and $\sigma_i = (\eta \cdot \omega) / \|A_{i:}\|_1$.
  3. Iteration loop computes:
     - Primal gradient step: $x^{k+1} = \Pi_{[l, u]}(x^k - \tau (c + A^T y^k))$.
     - Over-relaxation: $\bar{x}^{k+1} = 2 x^{k+1} - x^k$.
     - Dual step: $y^{k+1} = \Pi(y^k + \sigma A \bar{x}^{k+1})$.
     - Adaptive step-size reduction using local Lipschitz estimate $L_{local} = \|A \Delta x\| / \|\Delta x\|$.
     - Running average accumulation $(x_{avg}, y_{avg})$.
  4. Restart check every `restart_every` (40) iterations:
     - If `current_score <= 0.368 * last_restart_score`, resets to average iterate, updates primal weight $\omega = \omega \cdot (r_p / r_d)^{0.5}$.
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R1)**:
  - **No Stagnation Detection**: The solver has no sliding window tracking historical progress. If iterations continue without sufficient improvement, it simply runs until `max_iterations` and emits `iteration_limit`.
  - **No Basis Extraction or Crossover**: Requirement R1 mandates:
    `Implement stagnation detection (window = 1000, threshold = 0.999) in PDLP that triggers a basis extraction and crossover warm-start into dual simplex.`
    `PDLP crossover successfully resolves 100% of previously stalling Netlib test instances (kb2, lotfi, beaconfd) to certified KKT <= 10^-7.`
    Currently, zero crossover logic exists in `pdlp.cpp`.

---

### 4.3 ADMM Convex QP Solver (`src/qp/admm_solver.cpp`)
- **Headers**: `include/markov_cero/qp/admm_solver.hpp`, `include/markov_cero/qp/kkt.hpp`, `include/markov_cero/qp/model.hpp`
- **Implementation**: `src/qp/admm_solver.cpp` (367 lines), `src/qp/kkt.cpp` (310 lines).
- **Public Interface**:
  ```cpp
  struct QpOptions {
      double absolute_tolerance{1e-4};
      double relative_tolerance{1e-4};
      double primal_infeasible_tolerance{1e-5};
      double dual_infeasible_tolerance{1e-5};
      double sigma{1e-6};
      double rho_init{0.1};
      double alpha{1.6};
      std::size_t max_iterations{4000};
      double time_limit_seconds{60.0};
      bool adaptive_rho{true};
      std::size_t adaptive_rho_interval{25};
      bool verbose{false};
  };
  struct QpSolution {
      QpStatus status{QpStatus::numerical_error};
      double objective_value{0.0};
      std::vector<double> x;
      std::vector<double> z;
      std::vector<double> y;
      std::size_t iterations{0};
      double solve_time_seconds{0.0};
      double primal_residual{0.0};
      double dual_residual{0.0};
  };
  class AdmmQpSolver {
  public:
      explicit AdmmQpSolver(QpOptions options = {});
      [[nodiscard]] QpSolution solve(const QuadraticModel& model);
  };
  [[nodiscard]] QpSolution solve_qp(const QuadraticModel& model, const QpOptions& options = {});
  ```
- **Current Algorithm**:
  1. Convexity verification via $LDL^T$ factorization of $P + 10^{-12} I$.
  2. KKT system setup: $\begin{bmatrix} P + \sigma I & A^T \\ A & -\text{diag}(\rho)^{-1} \end{bmatrix} \begin{bmatrix} \tilde{x} \\ \nu \end{bmatrix} = \begin{bmatrix} \sigma x^k - q \\ z^k - \rho^{-1} y^k \end{bmatrix}$.
  3. Timothy Davis (2005) sparse $LDL^T$ quasi-definite factorization (`src/qp/kkt.cpp`).
  4. ADMM iteration:
     - Linear system solve via $L D L^T$.
     - Over-relaxation: $\hat{x}^{k+1} = \alpha \tilde{x}^{k+1} + (1-\alpha) x^k$, $\hat{z}^{k+1} = \alpha \tilde{z}^{k+1} + (1-\alpha) z^k$.
     - Projection $z^{k+1} = \Pi_{[l, u]}(\hat{z}^{k+1} + \rho^{-1} y^k)$.
     - Dual update $y^{k+1} = y^k + \rho (\hat{z}^{k+1} - z^{k+1})$.
     - Infeasibility certificate check (Banjac et al. 2019) every 10 iterations.
     - Adaptive $\rho$ check every 25 iterations: computes normalized primal/dual residuals $s_{prim}, s_{dual}$, updates $\rho = \text{clamp}(\rho \cdot \sqrt{s_{prim}/s_{dual}}, 10^{-3}, 10^4)$, refactorizes KKT with `kkt.update_numeric()`.
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R1)**:
  - **Penalty Parameter Clamping Range**: Currently clamped to $[10^{-3}, 10^4]$. Requirement R1 specifically demands:
    `Upgrade ADMM QP solver (src/qp/admm_solver.cpp) with Boyd et al. (2011) adaptive penalty parameter rho in [10^-6, 10^6] and refactorization counter.`
  - **Boyd et al. Adaptation Rule**: Boyd et al. Section 3.4.1 uses $\|r^k\|_2$ and $\|s^k\|_2$ with thresholds $\mu = 10, \tau^{incr} = 2, \tau^{decr} = 2$.
  - **Refactorization Counter Missing**: `QpSolution` does not track the number of $LDL^T$ refactorizations.

---

### 4.4 Sparse Basis & Linear Algebra (`src/linalg/sparse_basis.cpp`)
- **Headers**: `include/markov_cero/linalg/sparse_basis.hpp`
- **Implementation**: `src/linalg/sparse_basis.cpp` (719 lines).
- **Public Interface**:
  ```cpp
  struct SparseCsc {
      std::size_t rows{};
      std::size_t columns{};
      std::vector<std::size_t> column_offsets;
      std::vector<std::size_t> row_indices;
      std::vector<double> values;
      void validate(std::size_t maximum_nonzeros = 4U * 1024U * 1024U) const;
      [[nodiscard]] std::vector<double> dense_column(std::size_t column) const;
      [[nodiscard]] static SparseCsc from_columns(std::size_t rows, const std::vector<std::vector<double>>& columns);
  };
  class SparseLu final {
  public:
      static SparseLu factorize(const SparseCsc& matrix, double singular_tolerance = 1e-14,
                                std::size_t maximum_factor_nonzeros = 4U * 1024U * 1024U,
                                bool reduce_fill = true);
      [[nodiscard]] std::vector<double> solve(const std::vector<double>& rhs) const;
      [[nodiscard]] std::vector<double> solve_transpose(const std::vector<double>& rhs) const;
      [[nodiscard]] const SparseLuDiagnostics& diagnostics() const noexcept;
  };
  class SparseBasisFactorization final {
  public:
      static SparseBasisFactorization factorize(const SparseCsc& basis, const SparseBasisOptions& options = {});
      [[nodiscard]] std::vector<double> solve(const std::vector<double>& rhs);
      [[nodiscard]] std::vector<double> solve_transpose(const std::vector<double>& rhs);
      void replace_column(std::size_t position, const std::vector<double>& column);
      [[nodiscard]] bool needs_refactorization() const noexcept;
      void refactorize();
      [[nodiscard]] const SparseBasisStatistics& statistics() const noexcept;
      [[nodiscard]] bool refinement_required() const noexcept;
  };
  ```
- **Iterative Refinement Implementation**:
  - `residual_vector` (lines 528–550): Already calculates residuals in extended precision:
    `std::vector<long double> product(n, 0.0L);`
    `r[i] = static_cast<double>(static_cast<long double>(rhs[i]) - product[i]);`
  - `refinement_required` (lines 552–565):
    ```cpp
    bool SparseBasisFactorization::refinement_required() const noexcept {
        if (options_.maximum_refinement_steps == 0) return false;
        if (updates_.size() >= options_.refinement_trigger_updates) return true;
        const auto& diagnostics = base_.diagnostics();
        if (diagnostics.growth_factor > options_.refinement_trigger_growth) return true;
        if (sparse_condition_estimate(diagnostics) > options_.refinement_trigger_condition) return true;
        return false;
    }
    ```
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R1)**:
  - **Refinement is Conditional/Selective**: Currently only triggers if updates $\ge 16$, growth factor $> 100$, or condition $> 10^8$. Requirement R1 explicitly states:
    `Enforce always-on iterative refinement in src/linalg/sparse_basis.cpp with extended precision (long double) residual calculation.`

---

### 4.5 Model Representations, MPS Parser, SolveResult, and Diagnostics
- **Headers**:
  - `include/markov_cero/model/model.hpp`
  - `include/markov_cero/io/mps.hpp`
  - `include/markov_cero/api/solve.hpp`
- **Implementation**:
  - `src/model/model.cpp`
  - `src/io/mps.cpp`
  - `src/api/api.cpp`
- **Current `Model`**:
  ```cpp
  struct Model final {
      std::string name;
      ObjectiveSense objective_sense{ObjectiveSense::minimize};
      double objective_offset{0.0};
      SparseMatrixCSC matrix;
      std::vector<double> objective;
      std::vector<Bound> row_lower;
      std::vector<Bound> row_upper;
      std::vector<Bound> variable_lower;
      std::vector<Bound> variable_upper;
      std::vector<VariableType> variable_type;
      std::vector<std::string> row_name;
      std::vector<std::string> variable_name;
      bool has_quadratic_objective{false};
      SparseMatrixCSC quadratic_matrix;
      void validate() const;
  };
  ```
- **Current `SolveResult` in `include/markov_cero/api/solve.hpp`**:
  - Contains status, primal vector, objective, verification reports (`primal_report`, `canonical_report`), timing, iterations.
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R1 & R2)**:
  - **`NumericalDiagnostic` Missing**: Requirement R1:
    `Eliminate silent failures across all engines: emit a structured NumericalDiagnostic in SolveResult with primal/dual residuals, condition estimate, failure site, and suggested recovery flag.`
    `Every non-optimal or failed solve emits a structured NumericalDiagnostic containing valid residuals and guidance.`
    Currently, failures set status to `numerical_failure` or throw exceptions caught in `guarded()`, populating only `out.error = e.what()`. No structured condition estimate, residual vector, or failure site enum exists.
  - **Model Classifier Missing**: Requirement R2:
    `Build sovereign model classifier (src/model/classifier.cpp, include/markov_cero/model/classifier.hpp) determining problem class (LP, MILP, QP, MIQP, NLP, MINLP) from structural properties, MPS sections, and callback presence.`
    Currently, `src/api/api.cpp` lines 67–73 only uses a 5-line ad-hoc check:
    ```cpp
    if (out.resolved_engine == "auto") {
        if (model.has_quadratic_objective) {
            out.resolved_engine = has_discrete ? "miqp" : "qp";
        } else {
            out.resolved_engine = has_discrete ? "milp" : "primal";
        }
    }
    ```
    There is no `classifier.hpp` or `classifier.cpp`.

---

### 4.6 Nonlinear Programming (NLP) & MINLP (`src/nlp/`, `src/minlp/`)
- **Status**: Completely missing (directories do not exist).
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R3)**:
  - **SQP Solver**: `src/nlp/sqp_solver.cpp` and `include/markov_cero/nlp/sqp_solver.hpp` must be constructed with L-BFGS-B quasi-Newton Hessian approximation, Armijo/Wolfe line search on $\ell_1$ merit function, and KKT residual verifier ($\epsilon_{opt} \le 10^{-6}$).
  - **Convex MINLP Outer Approximation**: `src/minlp/minlp_solver.cpp` and `src/minlp/outer_approx.cpp` must be implemented using the SQP subproblem solver and master MILP branch-and-cut.
  - **Dual Input Modalities**:
    1. Programmatic C++ callback API (`NlpModel`).
    2. Custom MPS `NLOBJ` polynomial section parser (`src/io/nlobj_parser.cpp`).

---

### 4.7 GPU Subsystem (`gpu/`)
- **Headers & Sources**:
  - `gpu/include/markov_cero/gpu/device.hpp`, `gpu/src/device.cpp`
  - `gpu/kernels/pdhg_step.cu`, `gpu/kernels/reduce.cu`, `gpu/kernels/spmv.cu`, `gpu/kernels/vector_ops.cu`
  - `gpu/src/pdhg_step.cpp`, `gpu/src/spmv.cpp`, `gpu/src/buffer.cpp`, `gpu/src/csr.cpp`
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R2)**:
  - **Architecture Support**: `CMakeLists.txt` lines 18–20 has `75;80;86;89;90`. Needs expansion to `"all-major"` (sm_50 through sm_90).
  - **Compute Capability Verification**: `gpu/src/device.cpp` does not verify whether the runtime GPU's compute capability is supported by the binary.
  - **GPU ADMM Step Kernel**: `gpu/kernels/admm_step.cu` and `gpu/src/admm_matvec.cpp` do not exist. Needed for large QP instances ($NNZ(P) > 100,000$).
  - **Crossover Study Benchmark**: Scale instance generation up to 5M nonzeros and empirical crossover documentation (`evidence/benchmarks/crossover_study.csv`) needs completion.

---

### 4.8 Python Bindings (`python/`)
- **Status**: Completely missing.
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R4)**:
  - `python/_core/`: Zero-runtime-dependency `pybind11` CPython extension exposing `markov_cero.solve()`, `markov_cero.Model`, `markov_cero.NlpModel`, `markov_cero.SolveOptions`.
  - Zero-copy NumPy buffer protocol for solution vectors and sparse matrices.
  - `pyproject.toml` and CMake targets for `pip install .` workflows.

---

### 4.9 Machine Learning-Assisted Branching (`src/milp/ml_branching/`)
- **Status**: Completely missing (deferred in early prototypes per `STATUS.md`).
- **Identified Gaps vs `ORIGINAL_REQUEST.md` (R5)**:
  - Strong branching data logger `src/milp/ml_branching/training_logger.cpp` to record bipartite graph features and exact branch improvement scores on MIPLIB 2017 easy instances.
  - Strict ML best practices pipeline: fixed 70/15/15 train/val/test splits, normalizers fit on train only, ranking metrics (Kendall's $\tau$, NDCG@k) and MSE.
  - 2-layer bipartite GCN (~20k params), export to quantized Int8 ONNX (`data/ml_models/branching_scorer.onnx`).
  - Zero-dependency C++ inference (`src/milp/ml_branching/onnx_scorer.cpp`) active via `--branching ml_gnn`.

---

## 5. Complete CTest Target Inventory (59 Targets)

In the current build (`build/`), `ctest -N` lists 59 tests. All 44 core tests (#1 through #44) and 3 API tests (#51 through #53) pass 100%.

| # | CTest Name | Test Source File / Command | Category / Target Engine |
|---|---|---|---|
| 1 | `build_info` | `tests/build_info_test.cpp` | Foundation / Versioning |
| 2 | `model_verifier` | `tests/model_test.cpp` | Model / Verification Oracle |
| 3 | `mps_parser` | `tests/mps_parser_test.cpp` | IO / MPS Format |
| 4 | `lp_parser` | `tests/lp_parser_test.cpp` | IO / CPLEX LP Format |
| 5 | `mps_fuzz_smoke` | `tests/fuzz/mps_fuzz_smoke.cpp` | IO / Fuzz Regression Smoke |
| 6 | `model_properties` | `tests/model_property_test.cpp` | Model / Structural Invariants |
| 7 | `dense_lu` | `tests/dense_lu_test.cpp` | Linear Algebra / Dense LU |
| 8 | `primal_simplex` | `tests/primal_simplex_test.cpp` | LP / Primal Simplex |
| 9 | `primal_simplex_properties` | `tests/primal_simplex_property_test.cpp` | LP / Primal Simplex Invariants |
| 10 | `dual_simplex` | `tests/dual_simplex_test.cpp` | LP / Dual Simplex & Steepest Edge |
| 11 | `warm_start_properties` | `tests/warm_start_property_test.cpp` | LP / Warm-Start Reproducibility |
| 12 | `sparse_basis` | `tests/sparse_basis_test.cpp` | Linear Algebra / SparseLU Factorizer |
| 13 | `sparse_update_properties` | `tests/sparse_update_property_test.cpp` | Linear Algebra / Eta Updates |
| 14 | `audit_regressions` | `tests/regression_test.cpp` | Core / Bug Regression Suite |
| 15 | `sparse_canonicalize` | `tests/sparse_canonicalize_test.cpp` | Transform / Sparse Canonicalizer |
| 16 | `presolve` | `tests/presolve_test.cpp` | Presolve / 7 Reduction Rules |
| 17 | `ruiz_scaling` | `tests/ruiz_scaling_test.cpp` | Scaling / Ruiz Equilibration |
| 18 | `milp` | `tests/milp_test.cpp` | MILP / Branch-and-Cut Core |
| 19 | `milp_heuristics` | `tests/milp_heuristics_test.cpp` | MILP / Primal Heuristics & Pump |
| 20 | `milp_cuts` | `tests/milp_cuts_test.cpp` | MILP / GMI & MIR Cuts |
| 21 | `strong_branching` | `tests/strong_branching_test.cpp` | MILP / Strong Branching & Pseudo-Costs |
| 22 | `pdlp` | `tests/pdlp_test.cpp` | LP / First-Order PDHG Loop |
| 23 | `ipm` | `tests/ipm_test.cpp` | LP / Interior-Point & Crossover |
| 24 | `parallel_tree_search` | `tests/parallel_tree_search_test.cpp` | MILP / Multi-Threaded Search |
| 25 | `gpu_buffer` | `gpu/tests/gpu_buffer_test.cpp` | GPU / Memory Buffer Management |
| 26 | `equivalence` | `gpu/tests/equivalence_test.cpp` | GPU / CPU vs GPU Output Equivalence |
| 27 | `gpu_reduction` | `gpu/tests/reduction_test.cpp` | GPU / Parallel Reductions |
| 28 | `gpu_pdhg_step` | `gpu/tests/pdhg_step_test.cpp` | GPU / PDHG Kernel Step |
| 29 | `gpu_pdhg_restart` | `gpu/tests/pdhg_restart_test.cpp` | GPU / Restart Policies |
| 30 | `gpu_pdhg_adaptive` | `gpu/tests/pdhg_adaptive_test.cpp` | GPU / Adaptive Step Sizing |
| 31 | `gpu_pdhg_kkt` | `gpu/tests/pdhg_kkt_test.cpp` | GPU / KKT Convergence |
| 32 | `gpu_pdhg_timing` | `gpu/tests/pdhg_timing_test.cpp` | GPU / 4-Stage Kernel Timing |
| 33 | `qp` | `tests/qp_test.cpp` | QP / ADMM Solver & LDLT |
| 34 | `json_records` | Python script validating `.json` files | Diagnostics / Telemetry |
| 35 | `sovereignty_guard` | `scripts/check-sovereignty.py` | Compliance / Sovereignty Guard |
| 36 | `cli_blend_optimal` | `markov-cero-solve examples/blend.mps` | CLI / End-to-End Optimal LP |
| 37 | `cli_qp_portfolio` | `markov-cero-solve examples/qp_portfolio.mps` | CLI / End-to-End Convex QP |
| 38 | `cli_refinery_feasible` | `markov-cero-solve examples/refinery/refinery-feasible.mps` | CLI / Refinery Feasible |
| 39 | `cli_refinery_infeasible` | `markov-cero-solve ...-infeasible.mps` (WILL_FAIL) | CLI / Infeasibility Handling |
| 40 | `cli_refinery_malformed` | `markov-cero-solve ...-malformed.mps` (WILL_FAIL) | CLI / Parser Error Handling |
| 41 | `cli_refinery_limited` | `markov-cero-solve ... --iteration-limit 1` (WILL_FAIL) | CLI / Resource Limits |
| 42 | `cli_case_crude_oil` | `markov-cero-solve examples/cases/crude_oil_blending.mps` | Industrial Case / QP Blending |
| 43 | `cli_case_multiperiod` | `markov-cero-solve examples/cases/multiperiod_production.mps` | Industrial Case / MILP Production |
| 44 | `cli_case_supply_chain` | `markov-cero-solve examples/cases/supply_chain_logistics.mps` | Industrial Case / MILP Logistics |
| 45 | `domain_refinery_scheduling_large` | `markov-cero-solve data/cases/refinery_scheduling_large.mps` | Large Scale / Refinery Scheduling |
| 46 | `domain_crude_blending_large` | `markov-cero-solve data/cases/crude_blending_large.qps` | Large Scale / QP Blending |
| 47 | `domain_process_network_large` | `markov-cero-solve data/cases/process_network_large.mps` | Large Scale / Chemical Network |
| 48 | `domain_production_planning_large` | `markov-cero-solve data/cases/production_planning_large.mps --mip-gap 0.05` | Large Scale / MILP Planning |
| 49 | `domain_power_dispatch_dc_opf` | `markov-cero-solve data/cases/power_dispatch_dc_opf.qps` | Large Scale / DC Optimal Power Flow |
| 50 | `domain_supply_chain_large` | `markov-cero-solve data/cases/supply_chain_large.mps` | Large Scale / Supply Chain MILP |
| 51 | `api_demo` | `examples/api_demo.cpp` | API / C++ Demo Execution |
| 52 | `api_test` | `tests/api_test.cpp` | API / C++ Solve Unit Tests |
| 53 | `refinery_domain_and_iis` | `tests/refinery_test.cpp` | Domain / Refinery LP & IIS |
| 54 | `netlib_benchmarks` | `scripts/run_netlib.py` | Benchmark / Netlib Sweep |
| 55 | `miplib_benchmarks` | `scripts/run_miplib.py` | Benchmark / MIPLIB Sweep |
| 56 | `cut_effectiveness` | `scripts/measure_cut_effectiveness.py` | Benchmark / Cut Effectiveness |
| 57 | `compare_harness` | `scripts/run_compare.py` vs HiGHS | Comparative Benchmark Harness |
| 58 | `gpu_benchmarks` | `scripts/run_gpu.py` | Benchmark / GPU Acceleration |
| 59 | `gpu_profiling` | `scripts/profile_gpu.py` | Benchmark / GPU Profiling |

---

## 6. Comprehensive Traceability & Gap Matrix

| Milestone & Req | Specific Deliverables | Current Codebase Status | Exact Implementation Path & Action Required |
|---|---|---|---|
| **M1: R1 (IPM Sparse)** | Replace dense LU in `ipm.cpp` with sparse normal equations factorizer using `SparseLU` | Dense LU ($O(m^3)$) | Update `src/lp/interior/ipm.cpp` to form $A D A^T$ in `SparseCsc` and factorize via `linalg::SparseLu::factorize()`. |
| **M1: R1 (PDLP Crossover)** | Stagnation detection (window=1000, threshold=0.999), basis extraction, crossover warm-start into dual simplex | No stagnation detection, no crossover | Update `src/lp/first_order/pdlp.cpp` to track residual improvements over 1000 iterations; on stagnation, extract candidate basis and invoke `lp::dual::solve()`. |
| **M1: R1 (ADMM QP)** | Boyd et al. (2011) adaptive $\rho \in [10^{-6}, 10^6]$ and refactorization counter | Heuristic OSQP $\rho \in [10^{-3}, 10^4]$, no counter | Update `src/qp/admm_solver.cpp` and `QpSolution` struct with Boyd adaptation rules and refactorization count. |
| **M1: R1 (Refinement)** | Enforce always-on iterative refinement with `long double` residuals | Selective/conditional in `sparse_basis.cpp` | In `src/linalg/sparse_basis.cpp`, make `refinement_required()` return true (always perform at least 1 refinement step). |
| **M1: R1 (Diagnostics)** | Emit structured `NumericalDiagnostic` in `SolveResult` | Only string `error` | Add `NumericalDiagnostic` struct in `include/markov_cero/api/solve.hpp` with primal/dual residuals, condition estimate, failure site, suggested recovery. |
| **M2: R2 (Classifier)** | Sovereign model classifier determining LP, MILP, QP, MIQP, NLP, MINLP | 5-line ad-hoc check in `api.cpp` | Create `include/markov_cero/model/classifier.hpp` and `src/model/classifier.cpp`. |
| **M2: R2 (CUDA Arch)** | Broaden CUDA architectures to `"all-major"` (sm_50 through sm_90) | Only `"75;80;86;89;90"` | Update `CMakeLists.txt` CUDA architectures list. |
| **M2: R2 (Device CC Check)** | Runtime GPU compute capability verification | Only checks device count | Update `gpu/src/device.cpp` `get_device_info()` and `is_gpu_available()` to check compatibility. |
| **M2: R2 (GPU ADMM)** | GPU-accelerated ADMM step kernel for large QP ($NNZ(P) > 100,000$) | Only PDHG kernels exist | Create `gpu/kernels/admm_step.cu` and `gpu/src/admm_matvec.cpp`. |
| **M2: R2 (Crossover Study)** | Synthetic scale instances up to 5M nonzeros and crossover study | Up to 150k nonzeros | Run synthetic scaling scripts and document crossover in `evidence/benchmarks/crossover_study.csv`. |
| **M3: R3 (SQP)** | SQP solver with L-BFGS-B, Armijo/Wolfe line search, KKT verifier ($\le 10^{-6}$) | Missing | Create `include/markov_cero/nlp/sqp_solver.hpp` and `src/nlp/sqp_solver.cpp`. |
| **M3: R3 (MINLP OA)** | Convex MINLP Outer Approximation using SQP and master MILP | Missing | Create `include/markov_cero/minlp/minlp_solver.hpp`, `src/minlp/minlp_solver.cpp`, `src/minlp/outer_approx.cpp`. |
| **M3: R3 (NLP Inputs)** | Programmatic C++ `NlpModel` callback API and MPS `NLOBJ` parser | Missing | Create `include/markov_cero/nlp/nlp_model.hpp` and `src/io/nlobj_parser.cpp`. |
| **M4: R4 (Python Bindings)** | `pybind11` CPython extension in `python/_core/`, zero-copy NumPy buffers, `pyproject.toml` | Missing | Author Python package in `python/`, configure `pyproject.toml` and CMake targets. |
| **M5: R5 (ML Branching)** | Strong branching data logger, bipartite GCN Int8 ONNX inference, `--branching ml_gnn` | Missing | Create `src/milp/ml_branching/training_logger.cpp`, train model, write zero-dependency `src/milp/ml_branching/onnx_scorer.cpp`. |
| **M6: R6 (Comparison)** | Automated download of Netlib, QPLIB, automated comparative harness, Dolan-Moré profiles | Partial runners | Create `scripts/run_full_compare.py`, benchmark against HiGHS, GLPK, CBC, SCIP, generate SVG profiles. |

---

## 7. Recommended Work Packages & Dependencies

```
[Survey & Ground Truth] (Completed)
        │
        ▼
[Milestone 1: Numerical Hardening]
  ├── Sparse Normal Equations in IPM (SparseLU)
  ├── PDLP Stagnation Detection & Simplex Crossover
  ├── ADMM QP Boyd Adaptive rho & Refactorization Counter
  ├── Always-on Iterative Refinement
  └── Structured NumericalDiagnostic in SolveResult
        │
        ▼
[Milestone 2: Classifier & GPU Polish]
  ├── Problem Classifier (LP, MILP, QP, MIQP, NLP, MINLP)
  ├── CUDA "all-major" (sm_50 - sm_90) + Runtime CC Check
  └── GPU ADMM Step Kernel for Large QP
        │
        ▼
[Milestone 3: Nonlinear & MINLP]
  ├── SQP Solver (L-BFGS-B, line search, KKT verifier)
  ├── MINLP Outer Approximation
  └── NlpModel Callback API & NLOBJ MPS Parser
        │
        ▼
[Milestone 4: Sovereign Python Bindings]
  ├── pybind11 Zero-Copy NumPy Extension
  └── pyproject.toml / pip install .
        │
        ▼
[Milestone 5: ML-Assisted Branching]
  ├── Strong Branching Data Logger (Fixed 70/15/15 splits)
  ├── Bipartite GCN Int8 ONNX Model
  └── Zero-Dependency C++ Scorer (--branching ml_gnn)
        │
        ▼
[Milestone 6: Full Datasets & Benchmark Comparison]
  ├── Automated Netlib / MIPLIB / QPLIB Download & Verification
  ├── Comparative Benchmark Harness (scripts/run_full_compare.py)
  └── Dolan-Moré Performance Profiles
```
