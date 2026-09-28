# End-to-End (E2E) Test Infrastructure Specification: markov-cero

**Document Status**: Authoritative Specification & Architecture  
**Author**: `test_writer_e2e_1` (Teamwork E2E Testing Track)  
**Target Codebase**: `markov-cero` (SIH26119 Clean-Room C++20 Mathematical Optimization Solver Core)  
**Parent Requirements**: `/home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md`  
**Project Scope & Inventory**: `/home/saikrishna/markov-initial-build/PROJECT.md`  
**Last Updated**: 2026-09-27  

---

## 1. Executive Summary & Purpose

`markov-cero` is an in-tree, clean-room sovereign C++20 mathematical optimization solver core supporting LP (Primal Simplex, Dual Simplex, SparseLU IPM, First-Order PDLP with crossover), Convex QP/MIQP (Active Set, ADMM with adaptive penalty $\rho$, KKT refactorization, GPU step acceleration), NLP / MINLP (Sovereign SQP with L-BFGS-B, Outer Approximation MINLP), MILP Branch-and-Cut (GMI/MIR cuts, strong branching, heuristics, parallel tree search, ML-assisted bipartite GCN branching via zero-dependency C++ Int8 inference), and sovereign Python bindings.

The **E2E Testing Track** provides an independent, opaque-box, requirement-driven verification harness that guarantees solver correctness, numerical stability, interface contract compliance, and regression prevention. This document defines:
1. The **Testing Philosophy** governing all tests in `markov-cero`.
2. The **Authoritative Expected Output Derivation** protocol.
3. The **4-Tier Test Architecture** mapping all 38 inventoried features from `PROJECT.md` into:
   - **Tier 1**: Feature Coverage ($\ge 5$ test cases per feature, $\ge 190$ tests total)
   - **Tier 2**: Boundary & Corner Cases ($\ge 5$ test cases per feature, $\ge 190$ tests total)
   - **Tier 3**: Cross-Feature Combinations (Pairwise coverage, $\ge 38$ integration tests)
   - **Tier 4**: Real-World Application Scenarios ($\ge 19$ complex domain applications)
4. The **Test Harness Architecture & Runner Design** (`scripts/run_e2e_tests.py` and CTest integration).
5. The **Progressive Testability and Milestone Delivery Plan**.

---

## 2. Testing Philosophy & Principles

### 2.1 Opaque-Box & Requirement-Driven Testing
All tests in the E2E track adhere strictly to **opaque-box principles**:
- **Interface Decoupling**: Solvers and linear algebra engines are tested purely through public API boundaries (`markov_cero::api::solve_model`, `solve_file`, `AdmmQpSolver`, `SparseLu`, `NlpModel`, `SqpSolver`, `MinlpSolver`, CLI binaries, and Python bindings). Tests do not probe private fields or make assumptions about internal class layout.
- **Mathematical Invariant Verification**: Correctness is verified against mathematical ground truth:
  - Primal feasibility: $\|A x - b\|_\infty \le \epsilon_{\text{prim}}$ and bounds $l \le x \le u$.
  - Dual feasibility: $\|A^T y + s - c\|_\infty \le \epsilon_{\text{dual}}$ and dual bounds / signs.
  - Complementary slackness: $|x_j s_j| \le \epsilon_{\text{comp}}$ for all $j$.
  - Optimality gap: $\frac{|c^T x - b^T y|}{\max(1.0, |c^T x|)} \le \epsilon_{\text{gap}}$.
  - Karush-Kuhn-Tucker (KKT) stationarity and certificate checks.
- **Zero Silent Failures**: Every solve that is non-optimal or encounters numerical difficulty must emit an explicit status code and a structured `NumericalDiagnostic` containing valid residuals and guidance.

### 2.2 Clean-Room Sovereignty & Self-Containment
- **Zero Prohibited Dependencies**: In accordance with SIH 2026 clean-room sovereignty and `scripts/check-sovereignty.py`, all test fixtures, assertion macros, and test runners must not link or use prohibited libraries (e.g., `gtest`, `gmock`, `boost`, `eigen`, `fmt`, `spdlog`, `nlohmann/json`, `libtorch`, `onnxruntime`).
- **Standard C++20 & Standard Python**: Test executables are pure C++20 compiled with `-Wall -Wextra -Wpedantic -Werror`. The test runner script `scripts/run_e2e_tests.py` uses only Python standard library modules (`json`, `subprocess`, `sys`, `pathlib`, `argparse`, `xml.etree.ElementTree`).
- **Complete Test Isolation**: Every test is fully self-contained. Tests construct their own data, manage their own memory, and clean up temporary artifacts. No test depends on the execution state or ordering of preceding tests.

### 2.3 Progressive Testability
During milestone execution, test suites are authored such that each test is verifiable using **only features from the current milestone and its completed dependencies**. Forward dependencies are quarantined by milestone tags (`M1`, `M2`, ..., `M6`) and executed dynamically as implementation milestones reach completion.

---

## 3. Authoritative Expected Output Derivation

For every single test case authored in this track, the expected output must be derived from one of four authoritative sources:

| Source Category | Description | Application in markov-cero |
|-----------------|-------------|----------------------------|
| **1. Analytical Closed-Form Solutions** | Mathematically derived exact solutions obtained via calculus, linear algebra, or explicit duality theorems. | Unconstrained convex QPs ($x^* = -P^{-1}q$); small 2-variable / 3-variable toy LPs with known unique vertices; orthogonal projections; diagonal and triangular linear systems; standard benchmark functions (e.g., Rosenbrock minimum $f(1, 1) = 0$). |
| **2. Sovereign Reference Oracle** | Execution of the verified clean-room primal/dual revised simplex reference engines (`markov_cero::lp::reference::solve_revised_simplex`) and zero-trust verifiers (`verify_primal`, `verify_reference`, `verify_qp_solution`). | Used as a certified baseline oracle for IPM crossover verification, presolve postsolve round-trip equivalence, Ruiz scaling unscaling, and PDLP solutions. |
| **3. Certified Mathematical Invariants** | Invariant properties that must hold true regardless of algorithm trajectory (KKT conditions, residual tolerances, duality theorems, monotonicity of objective under cutting planes). | Residual norms $\|Ax - b\|_\infty \le 10^{-6}$, complementary slackness $\|x \circ s\|_\infty \le 10^{-6}$, condition estimates $\kappa \ge 1.0$, refactorization counters $> 0$ upon severe residual imbalance. |
| **4. Published Benchmark Ground Truth** | Certified optimum objective values from authoritative international mathematical programming libraries: Netlib LP (97 instances), MIPLIB 2017 easy benchmark instances, Mittelmann LP/QP benchmark tables, and convex QPLIB instances. | Validates full solver runs on standard instances (e.g. `blend.mps` obj = $-30.8125$, `afiro.mps` obj = $-464.7531$, `adlittle.mps` obj = $225494.963$, `stein15.mps` optimal obj = $9.0$). |

---

## 4. 4-Tier Test Architecture

The E2E test suite is organized into four hierarchical tiers:

```
+-----------------------------------------------------------------------------------+
|                        Tier 4: Real-World Applications                           |
|       (>=19 complex realistic domain scenarios: refinery, logistics, OPF, etc.)    |
+-----------------------------------------------------------------------------------+
|                     Tier 3: Cross-Feature Combinations                            |
|             (>=38 pairwise integration tests between interacting features)         |
+-----------------------------------------------------------------------------------+
|                     Tier 2: Boundary & Corner Cases                               |
|        (>=5 cases per feature -> >=190 tests: degeneracy, rank deficiency, scale)  |
+-----------------------------------------------------------------------------------+
|                        Tier 1: Feature Coverage                                   |
|      (>=5 cases per feature -> >=190 tests: nominal happy path across 38 features) |
+-----------------------------------------------------------------------------------+
```

### 4.1 Summary of Test Volume
- **Tier 1 (Feature Coverage)**: 38 features $\times 5$ tests minimum = **190 tests**
- **Tier 2 (Boundary & Corner Cases)**: 38 features $\times 5$ tests minimum = **190 tests**
- **Tier 3 (Cross-Feature Combinations)**: Pairwise feature interactions = **38 tests**
- **Tier 4 (Real-World Applications)**: Complex realistic models = **19 tests**
- **Total Minimum Test Count**: $190 + 190 + 38 + 19 =$ **437 test cases**

---

## 5. Comprehensive Feature Inventory Mapping

Below is the complete mapping of all 38 features from `PROJECT.md` across Tier 1, Tier 2, Tier 3, and Tier 4.

---

### Milestone 1: Numerical Accuracy Hardening (Features 1–7)

#### Feature 1: SparseLU IPM Normal Equations
- **Description**: Sparse normal equations factorizer using `SparseLu` for $A D A^T$ in interior point method, scaling IPM from $m \approx 200$ to $m \ge 50,000$.
- **Source**: ORIGINAL_REQUEST §R1, D-14
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F01-01`: Factorize positive definite normal equations $(A D A^T)$ with $D > 0$ and verify solution against dense solve.
  - `T1-F01-02`: Solve standard 3-variable LP via IPM with SparseLU and confirm optimal objective matches simplex.
  - `T1-F01-03`: Solve Netlib `sc50a` LP via SparseLU IPM and verify convergence to within $10^{-6}$ of reference.
  - `T1-F01-04`: Verify fill-reducing minimum-degree column ordering preserves solution vector within $10^{-12}$.
  - `T1-F01-05`: Solve LP with diagonal constraint matrix and verify SparseLU solves in linear time $O(m)$.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F01-01`: Factorize $(A D A^T)$ where some diagonal entries $D_{jj} \to 10^{12}$ and others $D_{jj} \to 10^{-12}$ (extreme IPM barrier iterates).
  - `T2-F01-02`: Single-row constraint matrix $1 \times n$ (extreme aspect ratio) solved via SparseLU normal equations.
  - `T2-F01-03`: High-density normal equations ($> 80\%$ fill) to verify stability and factor memory limit guards.
  - `T2-F01-04`: Structurally singular normal equations ($A$ rank deficient) to verify graceful error report without segfault.
  - `T2-F01-05`: Empty constraint matrix ($0 \times n$) normal equations handling.

#### Feature 2: PDLP Stagnation Detection
- **Description**: Windowed stagnation tracking (window=1000, threshold=0.999) in `pdlp.cpp`.
- **Source**: ORIGINAL_REQUEST §R1, D-15
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F02-01`: Verify stagnation detector triggers after 1000 iterations when candidate improvement $< 0.1\%$.
  - `T1-F02-02`: Verify stagnation detector does NOT trigger when candidate improvement $> 0.1\%$ per window.
  - `T1-F02-03`: Solve Netlib `kb2` and verify stagnation is correctly detected within 5000 iterations.
  - `T1-F02-04`: Verify stagnation trigger emits structured status `stagnation_detected` before crossover.
  - `T1-F02-05`: Verify window ring buffer maintains correct moving average and max/min without numerical drift.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F02-01`: Iteration limit set smaller than stagnation window ($N_{\text{max}} = 500 < 1000$); verify clean exit without out-of-bounds access.
  - `T2-F02-02`: Improvement ratio exactly at threshold ($0.999000000$); test boundary decision logic.
  - `T2-F02-03`: Stagnation detection on an inherently cycling or oscillating non-convergent saddle-point problem.
  - `T2-F02-04`: Stagnation window resets properly when an adaptive step size restart occurs.
  - `T2-F02-05`: Stagnation on tiny 1-variable problem that converges in $< 50$ iterations (no spurious trigger).

#### Feature 3: PDLP Dual Simplex Crossover
- **Description**: Extract candidate basis from complementary slackness ($x_j > \epsilon, s_j \le \epsilon$) and warm-start dual simplex.
- **Source**: ORIGINAL_REQUEST §R1, D-15
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F03-01`: Candidate basis extraction on 2-variable LP extracts exact optimal basis indices.
  - `T1-F03-02`: Dual simplex warm-start successfully purges primal infeasibilities and reaches vertex optimum.
  - `T1-F03-03`: Crossover resolves Netlib `lotfi` to certified KKT residual $\le 10^{-7}$.
  - `T1-F03-04`: Crossover resolves Netlib `beaconfd` to certified KKT residual $\le 10^{-7}$.
  - `T1-F03-05`: Verify `crossover_applied = true` and valid `BasisState` returned in `Result`.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F03-01`: Degenerate optimal iterate where multiple variables have $x_j \approx \epsilon, s_j \approx \epsilon$; test rank-revealing completion.
  - `T2-F03-02`: Infeasible candidate basis extracted; verify dual simplex recovers or detects infeasibility cleanly.
  - `T2-F03-03`: Problem with equality constraints (no slack columns in canonical form); verify basis dimension matches rows.
  - `T2-F03-04`: Primal-unbounded problem passed to crossover; verify unbounded ray detected.
  - `T2-F03-05`: Zero iterations of PDLP before crossover (cold basis extraction).

#### Feature 4: ADMM Adaptive Penalty $\rho$
- **Description**: Update $\rho \in [10^{-6}, 10^6]$ adaptively following Boyd et al. (2011) with $\mu = 10, \tau_{\text{incr}} = 2, \tau_{\text{decr}} = 2$.
- **Source**: ORIGINAL_REQUEST §R1, D-16
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F04-01`: Primal residual dominance ($\|r_{\text{prim}}\|_\infty > 10 \|r_{\text{dual}}\|_\infty$) doubles $\rho$.
  - `T1-F04-02`: Dual residual dominance ($\|r_{\text{dual}}\|_\infty > 10 \|r_{\text{prim}}\|_\infty$) halves $\rho$.
  - `T1-F04-03`: Balanced residuals ($\frac{1}{10} \le \frac{\|r_{\text{prim}}\|}{\|r_{\text{dual}}\|} \le 10$) keep $\rho$ unchanged.
  - `T1-F04-04`: Solve convex QP with poorly scaled constraints and verify adaptive $\rho$ converges in fewer iterations than fixed $\rho$.
  - `T1-F04-05`: Verify adaptive update occurs only on designated interval (e.g. every 25 iterations).
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F04-01`: $\rho$ reaches upper clamp ceiling ($10^6$) and does not overflow or increase further.
  - `T2-F04-02`: $\rho$ reaches lower clamp floor ($10^{-6}$) and does not underflow or decrease further.
  - `T2-F04-03`: Zero primal or dual residual ($r = 0$) handling without division by zero.
  - `T2-F04-04`: Quadratic problem with $P = 0$ (linear objective); verify adaptive $\rho$ remains stable.
  - `T2-F04-05`: ADMM QP with initial $\rho_{\text{init}} = 10^6$ at extreme upper limit.

#### Feature 5: ADMM KKT Refactorization Counter
- **Description**: Track KKT matrix refactorizations in `QpSolution.refactorization_count` triggered by $\rho$ updates.
- **Source**: ORIGINAL_REQUEST §R1, D-16
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F05-01`: Fixed $\rho$ solve (`adaptive_rho = false`) yields exactly 1 initial factorization.
  - `T1-F05-02`: Verify `refactorization_count` increments by 1 for each adaptive $\rho$ update.
  - `T1-F05-03`: Verify refactorization count is serialized and accessible in `QpSolution`.
  - `T1-F05-04`: Verify LDLT factorization cache correctly updates numerical values on refactorization without memory leaks.
  - `T1-F05-05`: Compare total solve time vs refactorization count to verify refactorization overhead is bounded.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F05-01`: Model where initial factorization fails due to non-convexity; verify counter is 0 or 1.
  - `T2-F05-02`: Solve that hits iteration limit before any $\rho$ update; verify count is 1.
  - `T2-F05-03`: Solve with 0 variables; verify refactorization counter is 0.
  - `T2-F05-04`: Rapid oscillations in residual ratio triggering refactorization every interval up to maximum iterations.
  - `T2-F05-05`: Refactorization counter behavior when solve is interrupted by time limit.

#### Feature 6: Always-On Iterative Refinement
- **Description**: Extended-precision `long double` iterative refinement pass with $\|r\|_\infty < 10^{-14}$ early exit in `SparseBasisFactorization`.
- **Source**: ORIGINAL_REQUEST §R1, D-17
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F06-01`: Solve well-conditioned linear system; verify refinement executes and achieves $\|r\|_\infty < 10^{-14}$.
  - `T1-F06-02`: Solve ill-conditioned linear system ($\kappa \approx 10^8$); verify refinement drops residual by $\ge 10^2$.
  - `T1-F06-03`: Verify early exit triggers immediately when initial residual already $< 10^{-14}$.
  - `T1-F06-04`: Verify `refinement_attempts` and `refinements_applied` counters increment in `SparseBasisStatistics`.
  - `T1-F06-05`: Verify extended precision `long double` residual computation prevents catastrophic cancellation.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F06-01`: Near-singular basis ($\kappa \approx 10^{15}$); verify refinement terminates gracefully without infinite looping.
  - `T2-F06-02`: Zero right-hand side vector $b = 0$; verify refinement exits immediately with $x = 0$.
  - `T2-F06-03`: Single $1 \times 1$ system $[10^{-12}] x = [10^{-12}]$; verify refinement precision.
  - `T2-F06-04`: Refinement with maximum allowed steps set to 1 (`max_steps = 1`).
  - `T2-F06-05`: Refinement on basis with long update chain (product-form ETA vectors $\ge 32$).

#### Feature 7: Structured NumericalDiagnostic
- **Description**: Emit structured `NumericalDiagnostic` in `SolveResult` with primal/dual residuals, condition estimate, failure site, and suggested recovery.
- **Source**: ORIGINAL_REQUEST §R1, C-3
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F07-01`: Optimal solve populates valid non-zero residuals and condition estimate $\ge 1.0$.
  - `T1-F07-02`: Ill-conditioned solve populates `condition_estimate > 1e12` and recovery advice (`enable_scaling` or `perturb_pivot`).
  - `T1-F07-03`: Infeasible solve populates failure site indicating infeasibility certificate detection.
  - `T1-F07-04`: Numerical factorization failure populates failure site (e.g. `SparseLu::factorize`) and recovery `use_dual_simplex`.
  - `T1-F07-05`: Verify `NumericalDiagnostic` is serialized in JSON outputs of `markov-cero-solve`.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F07-01`: Completely empty problem ($0 \times 0$); verify `NumericalDiagnostic` has default valid values without segfault.
  - `T2-F07-02`: NaN or infinite values in matrix entries; verify `NumericalDiagnostic` captures site `input_validation`.
  - `T2-F07-03`: Solver timeout; verify `NumericalDiagnostic` records partial residuals at termination.
  - `T2-F07-04`: Condition number estimate on orthogonal matrix (exact condition number $1.0$).
  - `T2-F07-05`: Multiple sequential solves using the same `SolveResult` structure; verify clean overwrite.

---

### Milestone 2: Problem Classification & GPU Polish (Features 8–13)

#### Feature 8: Sovereign Problem Classifier
- **Description**: Identify `LP`, `MILP`, `QP`, `MIQP`, `NLP`, `MINLP` from structural properties, MPS sections, and callbacks.
- **Source**: ORIGINAL_REQUEST §R2, D-02
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F08-01`: Model with continuous variables, linear objective, linear constraints $\to$ classify as `LP`.
  - `T1-F08-02`: Model with continuous and binary/integer variables, linear constraints $\to$ classify as `MILP`.
  - `T1-F08-03`: Model with quadratic objective (`QUADOBJ` or $P \neq 0$), continuous variables $\to$ classify as `QP`.
  - `T1-F08-04`: Model with quadratic objective and integer variables $\to$ classify as `MIQP`.
  - `T1-F08-05`: Model with non-linear objective callbacks or `NLOBJ` section $\to$ classify as `NLP` or `MINLP`.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F08-01`: Model with $P = 0$ (all quadratic coefficients zero); verify classified as `LP` rather than `QP`.
  - `T2-F08-02`: Model with integer variables where all bounds are fixed $l_j = u_j$; verify integer reduction handling.
  - `T2-F08-03`: Model with non-convex quadratic objective; verify classifier flags potential non-convexity.
  - `T2-F08-04`: Model with 0 constraints and 0 variables; verify classified as `LP`.
  - `T2-F08-05`: MPS file with empty/comment-only sections; verify robust classification.

#### Feature 9: Classifier Engine Selection Rules
- **Description**: Auto-route problem classes based on dimensions and density thresholds.
- **Source**: ORIGINAL_REQUEST §R2, D-02
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F09-01`: Small LP ($m < 1000$) routes to `primal_simplex` or `dual_simplex`.
  - `T1-F09-02`: Massive sparse LP ($m > 10000, \text{density} < 0.01$) routes to `pdlp` or `sparse_ipm`.
  - `T1-F09-03`: Dense LP ($m > 500, \text{density} > 0.2$) routes to `dense_lu` / `ipm`.
  - `T1-F09-04`: Convex QP routes to `admm_qp` or `active_set_qp`.
  - `T1-F09-05`: MILP with combinatorial knapsacks routes to `branch_and_cut` with cut separation.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F09-01`: Dimensions exactly on boundary threshold ($m = 1000$); verify deterministic tie-breaking.
  - `T2-F09-02`: Density exactly 0.0 (empty constraint matrix); verify selection of trivial solver.
  - `T2-F09-03`: Single-column problem ($n = 1, m = 10000$); verify routing logic.
  - `T2-F09-04`: Explicit user override via `--engine` ignores auto-route selection.
  - `T2-F09-05`: Unavailable backend requested (e.g. GPU requested on CPU-only machine); fallback auto-route.

#### Feature 10: CUDA Architecture Broadening
- **Description**: Support `"all-major"` (sm_50 through sm_90) in `CMakeLists.txt`.
- **Source**: ORIGINAL_REQUEST §R2, D-07
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F10-01`: Verify CMake flags include `sm_50`, `sm_60`, `sm_70`, `sm_75`, `sm_80`, `sm_86`, `sm_89`, `sm_90`.
  - `T1-F10-02`: Compile test kernel targeting `sm_50` and verify binary contains corresponding PTX.
  - `T1-F10-03`: Compile test kernel targeting `sm_80` (Ampere) and verify binary compatibility.
  - `T1-F10-04`: Verify `MARKOV_CERO_ENABLE_CUDA=OFF` cleans all CUDA flags cleanly.
  - `T1-F10-05`: Verify sovereignty guard script approves broadened CUDA architecture list.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F10-01`: Invalid architecture string passed to CMake; verify build failure with clear error message.
  - `T2-F10-02`: Mixed CUDA compiler versions compatibility check.
  - `T2-F10-03`: Cross-compilation for foreign GPU compute target without attached device.
  - `T2-F10-04`: Verify PTX forward compatibility flag is set for architectures $> \text{sm\_90}$.
  - `T2-F10-05`: Single architecture override (`-DCMAKE_CUDA_ARCHITECTURES=75`) respected.

#### Feature 11: Runtime GPU Capability Guard
- **Description**: Verify `props.major >= 5` in `gpu/src/device.cpp` before allocating GPU buffers.
- **Source**: ORIGINAL_REQUEST §R2, D-07
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F11-01`: Verify `get_device_info()` queries and reports correct device name, major, minor, and VRAM.
  - `T1-F11-02`: On compute capability $\ge 5.0$, `is_gpu_available()` returns true.
  - `T1-F11-03`: On simulated capability $< 5.0$, `is_gpu_available()` returns false.
  - `T1-F11-04`: On no physical GPU present, runtime falls back gracefully to CPU without crash.
  - `T1-F11-05`: Verify GPU buffer allocation logs failure and initiates CPU fallback when guard fails.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F11-01`: Compute capability exactly 5.0 (sm_50 boundary condition).
  - `T2-F11-02`: Multi-GPU system with heterogeneous compute capabilities (one $< 5.0$, one $\ge 5.0$).
  - `T2-F11-03`: GPU memory exhaustion during capability verification; check exception handling.
  - `T2-F11-04`: CUDA driver version older than CUDA toolkit version.
  - `T2-F11-05`: Concurrently querying `get_device_info()` from multiple threads.

#### Feature 12: GPU ADMM Step Kernel
- **Description**: Accelerate $(P + \rho I) x$ in `gpu/kernels/admm_step.cu` and `gpu/src/admm_matvec.cpp` for large QP instances ($NNZ(P) > 100,000$).
- **Source**: ORIGINAL_REQUEST §R2, D-08
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F12-01`: Execute GPU ADMM step kernel and verify numerical equivalence with CPU matrix-vector multiply within $10^{-12}$.
  - `T1-F12-02`: Verify elementwise vector projection kernel ($z = \text{project}(x, l, u)$) produces identical result to CPU.
  - `T1-F12-03`: Verify dual update kernel ($y \leftarrow y + \rho(Ax - z)$) matches CPU update.
  - `T1-F12-04`: Solve 100k NNZ QP with GPU ADMM and confirm KKT convergence.
  - `T1-F12-05`: Profile kernel execution time to verify GPU speedup over CPU for $NNZ(P) > 100,000$.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F12-01`: Dense matrix represented in CSR format on GPU; verify thread block occupancy.
  - `T2-F12-02`: Matrix with rows containing 0 nonzeros; verify warp divergence handling.
  - `T2-F12-03`: Vectors containing $\pm \infty$ in bounds $[l, u]$; verify GPU clamp handles infinity.
  - `T2-F12-04`: Problem size exceeding GPU L2 cache ($> 64$ MB working set).
  - `T2-F12-05`: Host-to-device and device-to-host asynchronous stream synchronization under repeated iterations.

#### Feature 13: Empirical Crossover Benchmark
- **Description**: Scale instances up to 5M nonzeros; generate `evidence/benchmarks/crossover_study.csv`.
- **Source**: ORIGINAL_REQUEST §R2, D-08
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F13-01`: Generate synthetic test LP with 10k nonzeros and record solve time for Simplex, IPM, and PDLP.
  - `T1-F13-02`: Generate synthetic test LP with 100k nonzeros and record memory and iteration count.
  - `T1-F13-03`: Generate synthetic test LP with 1M nonzeros and verify crossover study records all metrics.
  - `T1-F13-04`: Verify `crossover_study.csv` contains valid headers (`m`, `n`, `nnz`, `engine`, `time_s`, `kkt_err`).
  - `T1-F13-05`: Verify crossover point identification logic detects the inflection where first-order surpasses simplex.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F13-01`: Extreme aspect ratios ($m = 10, n = 1,000,000$ and $m = 1,000,000, n = 10$) in scale generator.
  - `T2-F13-02`: Memory threshold guard preventing Out-Of-Memory (OOM) on systems with $< 16$ GB RAM.
  - `T2-F13-03`: Benchmark runner behavior when an individual scale solve times out ($> 300$s).
  - `T2-F13-04`: Verifying zero-variance results between successive runs with fixed random seed.
  - `T2-F13-05`: Synthetic instance with high condition number ($\kappa > 10^9$) in scale study.

---

### Milestone 3: Nonlinear & Mixed-Integer Nonlinear Programming (Features 14–20)

#### Feature 14: Sovereign SQP Solver Core
- **Description**: Construct sequential quadratic programming solver in `src/nlp/sqp_solver.cpp`.
- **Source**: ORIGINAL_REQUEST §R3, D-01
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F14-01`: Solve unconstrained Rosenbrock function and reach minimum $(1.0, 1.0)$ with $|f(x) - 0.0| < 10^{-3}$.
  - `T1-F14-02`: Solve constrained quadratic equality problem and reach known analytical optimum.
  - `T1-F14-03`: Solve problem with nonlinear inequality constraint ($x_1^2 + x_2^2 \le 1$) and linear objective.
  - `T1-F14-04`: Verify QP subproblem generation at each SQP iteration correctly linearizes constraints.
  - `T1-F14-05`: Verify SQP termination criterion exits when step norm $\|\Delta x\| < 10^{-6}$.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F14-01`: Incompatible QP subproblem (locally infeasible quadratic approximation); verify restoration phase.
  - `T2-F14-02`: Starting point directly at optimal point $x_0 = x^*$; verify 0-step convergence.
  - `T2-F14-03`: Highly nonlinear function with near-zero gradients (saddle points); verify Hessian regularization.
  - `T2-F14-04`: Maximum SQP iterations reached without convergence; emit `iteration_limit`.
  - `T2-F14-05`: SQP on strictly linear problem; verify equivalence with LP solution.

#### Feature 15: L-BFGS-B Hessian Approximation
- **Description**: Maintain compact two-loop quasi-Newton Hessian approximation ($m=10$).
- **Source**: ORIGINAL_REQUEST §R3, D-01
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F15-01`: Verify two-loop recursion accurately approximates Hessian-vector product on quadratic bowl.
  - `T1-F15-02`: Verify curvature condition $s_k^T y_k > 0$ is checked before storing displacement pairs.
  - `T1-F15-03`: Verify memory limit $m = 10$ displaces oldest vector pair when buffer is full.
  - `T1-F15-04`: Verify initial scaling matrix $\gamma_k = \frac{s_k^T y_k}{y_k^T y_k} I$ is applied accurately.
  - `T1-F15-05`: Verify restart mechanism resets history when negative curvature $s_k^T y_k \le 0$ is detected.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F15-01`: Zero step displacement $s_k = 0$; verify history skip without division by zero.
  - `T2-F15-02`: Memory buffer size set to $m = 1$ (minimal memory).
  - `T2-F15-03`: Memory buffer size set to $m = 50$ (large memory).
  - `T2-F15-04`: Consecutive identical displacement vectors $s_k = s_{k-1}, y_k = y_{k-1}$.
  - `T2-F15-05`: Orthogonal displacement and gradient change $s_k^T y_k = 0$.

#### Feature 16: Armijo-Wolfe Line Search
- **Description**: Backtracking line search with $\ell_1$ merit function and descent validation.
- **Source**: ORIGINAL_REQUEST §R3, D-01
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F16-01`: Armijo condition ($\phi(\alpha) \le \phi(0) + c_1 \alpha \phi'(0)$) satisfied on convex line search.
  - `T1-F16-02`: Wolfe curvature condition satisfied for full step $\alpha = 1.0$ when quadratic approximation is accurate.
  - `T1-F16-03`: Backtracking reduces step $\alpha \leftarrow \tau \alpha$ ($\tau = 0.5$) when Armijo condition fails.
  - `T1-F16-04`: Merit function penalty parameter $\mu$ increases automatically when constraint violation dominates.
  - `T1-F16-05`: Verify line search terminates when step size falls below $\alpha_{\text{min}} = 10^{-12}$.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F16-01`: Non-descent direction proposed by subproblem ($\phi'(0) \ge 0$); verify error recovery.
  - `T2-F16-02`: Discontinuous or non-differentiable function response; verify graceful step collapse.
  - `T2-F16-03`: Function evaluates to NaN or infinity along search ray; verify step reduction.
  - `T2-F16-04`: Extremely flat merit function gradient ($\phi'(0) \approx -10^{-16}$).
  - `T2-F16-05`: Line search with bounds restricting maximum step length $\alpha_{\text{max}} < 1.0$.

#### Feature 17: KKT Residual Verifier
- **Description**: Verify optimality conditions: stationarity, feasibility, complementarity $\le 10^{-6}$.
- **Source**: ORIGINAL_REQUEST §R3, D-01
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F17-01`: Stationarity violation $\|\nabla f(x) + J_g(x)^T \lambda + J_h(x)^T \nu\|_\infty \le 10^{-6}$ verified on optimal NLP.
  - `T1-F17-02`: Primal feasibility violation $\|h(x)\|_\infty \le 10^{-6}$ and $\max(0, g_i(x)) \le 10^{-6}$ verified.
  - `T1-F17-03`: Dual feasibility $\lambda_i \ge 0$ verified for inequality multipliers.
  - `T1-F17-04`: Complementary slackness $|\lambda_i g_i(x)| \le 10^{-6}$ verified.
  - `T1-F17-05`: Suboptimal candidate point fails verifier with explicit report on failing condition.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F17-01`: Active constraint with zero multiplier (strict complementarity failure); verify tolerance handling.
  - `T2-F17-02`: Multipliers with magnitude $> 10^{10}$ on ill-conditioned constraints.
  - `T2-F17-03`: Infeasible point with small stationarity error correctly fails feasibility check.
  - `T2-F17-04`: Problem with no inequality constraints (pure equality NLP).
  - `T2-F17-05`: Verifier evaluation on point with infinite objective value.

#### Feature 18: Convex MINLP Outer Approximation
- **Description**: Implement Outer Approximation in `src/minlp/minlp_solver.cpp` & `outer_approx.cpp`.
- **Source**: ORIGINAL_REQUEST §R3, D-03
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F18-01`: Solve small convex MINLP (e.g. integer variables in quadratic objective) to global optimality.
  - `T1-F18-02`: Master MILP problem generation adds supporting hyperplanes for nonlinear constraints.
  - `T1-F18-03`: NLP subproblem solves with fixed integer variables and provides primal upper bound.
  - `T1-F18-04`: Lower bound from master MILP and upper bound from NLP converge within gap tolerance $\le 10^{-4}$.
  - `T1-F18-05`: Integer feasibility verified for all integer-constrained variables in final solution.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F18-01`: Infeasible NLP subproblem for a specific integer assignment; verify feasibility cut generation.
  - `T2-F18-02`: MINLP where continuous relaxation is already integer-feasible (1-iteration convergence).
  - `T2-F18-03`: All variables declared integer (pure INLP).
  - `T2-F18-04`: Master MILP relaxation unbounded; verify bounding box enforcement.
  - `T2-F18-05`: Maximum Outer Approximation iterations reached; emit best incumbent found.

#### Feature 19: Programmatic NlpModel API
- **Description**: C++ callback interface with user-defined objective, gradient, constraints, and Jacobian.
- **Source**: ORIGINAL_REQUEST §R3, D-04
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F19-01`: Construct `NlpModel` with lambda callbacks for objective $f(x)$ and gradient $\nabla f(x)$.
  - `T1-F19-02`: Register equality and inequality constraint callbacks $g(x), h(x)$.
  - `T1-F19-03`: Register sparse Jacobian evaluation callback and verify coordinate alignment.
  - `T1-F19-04`: Verify `NlpModel::validate()` checks dimension consistency between bounds and variables.
  - `T1-F19-05`: Pass programmatic `NlpModel` to `SqpSolver` and verify end-to-end solve.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F19-01`: Missing gradient callback in `NlpModel`; verify explicit exception or finite-difference fallback.
  - `T2-F19-02`: Callback throws C++ `std::exception`; verify solver catches and returns `numerical_error` cleanly.
  - `T2-F19-03`: Jacobian sparsity pattern changes dynamically; verify structure invariance validation.
  - `T2-F19-04`: Variable bounds $l_j > u_j$ registered; verify `validate()` rejects model.
  - `T2-F19-05`: Zero constraint `NlpModel` (pure unconstrained optimization).

#### Feature 20: MPS NLOBJ Section Parser
- **Description**: Parse polynomial non-linear objective sections from extended MPS files.
- **Source**: ORIGINAL_REQUEST §R3, D-04
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F20-01`: Parse cubic polynomial term `NLOBJ X1 3 2.5` ($2.5 x_1^3$) into `NlpModel`.
  - `T1-F20-02`: Parse cross-product monomial terms `NLOBJ X1 1 X2 2 4.0` ($4.0 x_1 x_2^2$).
  - `T1-F20-03`: Evaluate parsed polynomial objective and analytic gradient on test vector.
  - `T1-F20-04`: Round-trip parse extended MPS file with standard linear constraints and `NLOBJ` section.
  - `T1-F20-05`: Solve parsed `NLOBJ` MPS model via `markov-cero-solve` CLI and verify result.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F20-01`: Degree 0 monomial (constant term in `NLOBJ`); verify added to objective offset.
  - `T2-F20-02`: Negative powers or malformed format in `NLOBJ`; verify graceful syntax error line report.
  - `T2-F20-03`: High degree monomial ($x^{10}$); test evaluation stability.
  - `T2-F20-04`: Empty `NLOBJ` section; verify treated as standard linear model.
  - `T2-F20-05`: Duplicate variable terms in `NLOBJ`; verify linear combination summation.

---

### Milestone 4: Sovereign Python Bindings (Features 21–24)

#### Feature 21: Sovereign pybind11 CPython Module
- **Description**: Build `python/_core/` CPython extension exposing solver API with zero runtime deps.
- **Source**: ORIGINAL_REQUEST §R4, D-05
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F21-01`: Import `markov_cero._core` in Python and inspect module metadata (`__version__`).
  - `T1-F21-02`: Verify shared library contains zero forbidden dynamic links via `ldd`.
  - `T1-F21-03`: Call `markov_cero._core.solve_file()` from Python on `examples/blend.mps`.
  - `T1-F21-04`: Pass `SolveOptions` dictionary or object from Python to C++ core.
  - `T1-F21-05`: Receive structured `SolveResult` in Python with status, objective, and solution arrays.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F21-01`: Pass invalid non-existent file path from Python; verify Python `FileNotFoundError` raised.
  - `T2-F21-02`: Python keyboard interrupt (`SIGINT` / `KeyboardInterrupt`) during solve handled cleanly.
  - `T2-F21-03`: Pass incompatible type for option fields (e.g. string for `num_threads`); verify `TypeError`.
  - `T2-F21-04`: Multi-threaded Python invocations releasing the Global Interpreter Lock (GIL).
  - `T2-F21-05`: Import and destroy module in rapid succession (module unload cleanup).

#### Feature 22: Python Model / NlpModel Bindings
- **Description**: Python classes `markov_cero.Model`, `NlpModel`, `SolveOptions`, and `solve()`.
- **Source**: ORIGINAL_REQUEST §R4, D-05
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F22-01`: Construct linear `Model` programmatically in Python, add rows/cols, solve, and assert obj.
  - `T1-F22-02`: Construct `NlpModel` in Python with Python callable objective and gradient, solve with SQP.
  - `T1-F22-03`: Set variable bounds, integer types, and objective sense in Python `Model`.
  - `T1-F22-04`: Extract dual variables and slacks through Python properties.
  - `T1-F22-05`: Solve QP model via Python `markov_cero.solve(qp_model)`.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F22-01`: Python callback in `NlpModel` raises Python exception; verify clean C++ translation.
  - `T2-F22-02`: Assign mismatched length lists to `row_lower` and `row_upper`; verify Python `ValueError`.
  - `T2-F22-03`: Empty model constructed and solved from Python.
  - `T2-F22-04`: Python garbage collection of callback while `NlpModel` is active; verify reference holding.
  - `T2-F22-05`: Unicode variable and constraint names handled correctly in Python API.

#### Feature 23: Zero-Copy NumPy Buffer Protocol
- **Description**: Direct buffer mapping for solution vectors, constraint matrices, and Jacobians.
- **Source**: ORIGINAL_REQUEST §R4, D-06
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F23-01`: Inspect solution vector `res.primal` as a NumPy ndarray without data copying.
  - `T1-F23-02`: Construct `SparseMatrixCSC` from NumPy arrays (`indptr`, `indices`, `data`) with buffer protocol.
  - `T1-F23-03`: Modify numpy array slice and verify memory view points directly to C++ buffer.
  - `T1-F23-04`: Evaluate Jacobian callback returning 2D NumPy array with zero intermediate copies.
  - `T1-F23-05`: Verify 1M-element vector pass between Python and C++ completes in $< 1$ microsecond.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F23-01`: Non-contiguous NumPy array passed (Fortran contiguous or strided); verify error or copy.
  - `T2-F23-02`: NumPy array with incorrect dtype (e.g. `float32` instead of `float64`); verify type check.
  - `T2-F23-03`: Zero-dimensional or empty NumPy array passed as constraint vector.
  - `T2-F23-04`: Read-only NumPy buffer protocol handling for immutable solution outputs.
  - `T2-F23-05`: Large array allocation memory stress test ($10^7$ doubles).

#### Feature 24: Pip Build Integration
- **Description**: Configure `pyproject.toml` and CMake targets for seamless `pip install .`.
- **Source**: ORIGINAL_REQUEST §R4, D-05
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F24-01`: Execute `pip install . --dry-run` or isolated build and verify build backend configuration.
  - `T1-F24-02`: Verify `pyproject.toml` specifies standard `scikit-build-core` or `setuptools`.
  - `T1-F24-03`: Verify wheel build produces compliant Linux tag (e.g. `manylinux` or native).
  - `T1-F24-04`: Run `pytest` on installed package in isolated virtual environment.
  - `T1-F24-05`: Verify header files and binaries are packaged in designated package locations.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F24-01`: Build with `--no-build-isolation` against pre-installed dependencies.
  - `T2-F24-02`: Pip install when CUDA is enabled vs disabled.
  - `T2-F24-03`: Build under non-standard Python environment versions (3.10, 3.11, 3.12).
  - `T2-F24-04`: Package installation in read-only target prefix without root permissions.
  - `T2-F24-05`: Clean uninstallation via `pip uninstall markov-cero`.

---

### Milestone 5: ML-Assisted Branching with ML Best Practices (Features 25–32)

#### Feature 25: Strong Branching Data Logger
- **Description**: Record bipartite graph features and branch scores on MIPLIB easy instances.
- **Source**: ORIGINAL_REQUEST §R5, D-10
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F25-01`: Log strong branching scores on small MILP node and verify recorded data rows.
  - `T1-F25-02`: Verify exact dual bound improvements $(\Delta z_j^+, \Delta z_j^-)$ are recorded.
  - `T1-F25-03`: Verify product score $s_j = \max(\Delta z_j^+, 10^{-6}) \times \max(\Delta z_j^-, 10^{-6})$ calculation.
  - `T1-F25-04`: Record candidate features across multiple branch-and-bound tree depths.
  - `T1-F25-05`: Verify logger writes structured CSV or HDF5/binary records with schema version header.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F25-01`: Node where strong branching detects child node infeasibility; verify score handling.
  - `T2-F25-02`: Problem with 0 integer fractional candidates (all integer feasible); verify 0 rows logged.
  - `T2-F25-03`: Logger file path invalid or directory not writable; verify clean exception without crash.
  - `T2-F25-04`: Candidate variable with 0 dual bound change on both branches ($\Delta z = 0$).
  - `T2-F25-05`: High node throughput logging under multi-threaded parallel tree search.

#### Feature 26: Strict ML Data Partitioning
- **Description**: Fixed instance-based 70% train, 15% validation, 15% test splits without data leakage.
- **Source**: ORIGINAL_REQUEST §R5, D-10
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F26-01`: Split MIPLIB instance set into disjoint 70/15/15 partitions by instance name.
  - `T1-F26-02`: Verify zero instance overlap between train, val, and test partitions ($Train \cap Val = \emptyset$).
  - `T1-F26-03`: Fit feature normalizer (mean, std) ONLY on training split and transform val/test.
  - `T1-F26-04`: Verify split manifest is recorded in deterministic JSON metadata file.
  - `T1-F26-05`: Verify test split is evaluated only once after model selection on validation split.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F26-01`: Total instance count not divisible cleanly by 100; verify rounding preserves disjoint sets.
  - `T2-F26-02`: Zero-variance feature on training set; verify normalizer avoids division by zero.
  - `T2-F26-03`: Outlier feature value in test set exceeding training range by $10^4$; test robust clipping.
  - `T2-F26-04`: Partitioning with small dataset ($< 10$ instances total).
  - `T2-F26-05`: Reproducibility: same random seed yields identical partitions across platforms.

#### Feature 27: Bipartite Graph Featurization
- **Description**: Strict feature vector ordering for constraint, variable, and edge representations.
- **Source**: ORIGINAL_REQUEST §R5, D-10
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F27-01`: Verify variable node feature vector contains exact documented fields in strict order.
  - `T1-F27-02`: Verify constraint node feature vector contains cosine, bias, and dual activity in strict order.
  - `T1-F27-03`: Verify edge weights correspond exactly to normalized constraint matrix entries $A_{ij}$.
  - `T1-F27-04`: Verify graph adjacency matrix matches bipartite structure of constraint matrix.
  - `T1-F27-05`: Verify feature extraction time is $< 5\%$ of total LP relaxation time.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F27-01`: Isolated variable with no constraint connections; verify graph degree 0 handling.
  - `T2-F27-02`: Empty row (all constraint coefficients zero); verify row feature handling.
  - `T2-F27-03`: Massive bipartite graph ($> 100,000$ edges) featurization without memory blowup.
  - `T2-F27-04`: Extreme coefficient range ($|A_{ij}| \in [10^{-10}, 10^{10}]$) normalization.
  - `T2-F27-05`: Featurization at root node vs deep tree node (depth feature updating).

#### Feature 28: ML Ranking & Regression Metrics
- **Description**: Evaluate Kendall's $\tau$, NDCG@k, and MSE during training.
- **Source**: ORIGINAL_REQUEST §R5, D-10
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F28-01`: Compute Kendall's $\tau$ rank correlation on identical rankings and assert $\tau = 1.0$.
  - `T1-F28-02`: Compute Kendall's $\tau$ on inverse rankings and assert $\tau = -1.0$.
  - `T1-F28-03`: Compute NDCG@5 on predicted scores vs ground truth strong branching ranking.
  - `T1-F28-04`: Compute Mean Squared Error (MSE) on score regression predictions.
  - `T1-F28-05`: Generate confusion matrix for binary top-1 candidate selection accuracy.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F28-01`: Tied ranks in ground truth scores; verify Kendall's $\tau_b$ tie handling.
  - `T2-F28-02`: Fewer candidate variables than $k$ in NDCG@k ($N_{\text{cand}} < k$).
  - `T2-F28-03`: Candidate scores all identical; verify zero rank correlation undefined case handled.
  - `T2-F28-04`: Extreme prediction outlier ($10^6$ error) in MSE calculation.
  - `T2-F28-05`: Single candidate variable ($N_{\text{cand}} = 1$); verify NDCG = 1.0.

#### Feature 29: 2-Layer Bipartite GCN Model
- **Description**: Train ~20k parameter bipartite GCN in PyTorch.
- **Source**: ORIGINAL_REQUEST §R5, D-11
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F29-01`: Forward pass on bipartite graph produces score vector with dimension equal to candidate count.
  - `T1-F29-02`: Verify total trainable parameter count is $20,000 \pm 2,000$.
  - `T1-F29-03`: Verify message passing updates variable representations from constraint neighbors.
  - `T1-F29-04`: Verify message passing updates constraint representations from variable neighbors.
  - `T1-F29-05`: Verify loss backward pass computes gradients without NaN.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F29-01`: Batch size = 1 forward and backward pass.
  - `T2-F29-02`: Disconnected bipartite graph components; verify message passing stability.
  - `T2-F29-03`: Activation function saturation behavior (ReLU / LeakyReLU).
  - `T2-F29-04`: Gradient clipping thresholding when loss spikes.
  - `T2-F29-05`: Model evaluation in inference mode (`eval()`) with disabled dropout.

#### Feature 30: Quantized Int8 ONNX Export
- **Description**: Export model to `data/ml_models/branching_scorer.onnx` with calibration.
- **Source**: ORIGINAL_REQUEST §R5, D-11
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F30-01`: Export PyTorch GCN to ONNX format and verify file exists at designated path.
  - `T1-F30-02`: Verify ONNX model contains Int8 quantized weight operators.
  - `T1-F30-03`: Verify ONNX model input/output tensor shapes and types match featurization contracts.
  - `T1-F30-04`: Verify quantization calibration error is $< 5\%$ compared to FP32 model predictions.
  - `T1-F30-05`: Verify exported ONNX file size is $< 100$ KB.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F30-01`: Quantization clipping on dynamic range outliers.
  - `T2-F30-02`: Zero tensor inputs to quantized ONNX graph.
  - `T2-F30-03`: ONNX model validation against ONNX specification standard checker.
  - `T2-F30-04`: Variable graph sizes (dynamic input shapes for constraint and variable counts).
  - `T2-F30-05`: SHA-256 checksum verification of exported ONNX artifact.

#### Feature 31: Zero-Dependency C++ Inference
- **Description**: Sovereign pure C++20 Int8 GCN evaluator in `src/milp/ml_branching/onnx_scorer.cpp`.
- **Source**: ORIGINAL_REQUEST §R5, D-12
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F31-01`: Execute C++ Int8 GCN inference and verify output matches ONNX reference within quantization tolerance.
  - `T1-F31-02`: Verify zero external libraries linked (`nm` checks show no onnxruntime or libtorch).
  - `T1-F31-03`: Verify integer arithmetic and SIMD / AVX2 vectorization acceleration for matrix multiplication.
  - `T1-F31-04`: Verify candidate ranking produced by C++ inference matches python model top-k choices.
  - `T1-F31-05`: Benchmark inference latency: verify average node inference time $< 1.0$ ms.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F31-01`: Integer overflow in Int8 $\times$ Int8 accumulation; verify Int32 accumulator prevents overflow.
  - `T2-F31-02`: Missing or corrupt weights binary file; verify clear error report and fallback.
  - `T2-F31-03`: Candidate count = 1; verify immediate trivial selection without inference cost.
  - `T2-F31-04`: Single thread vs multi-threaded concurrent inference evaluations from parallel tree search.
  - `T2-F31-05`: Extreme graph size (50k variables) inference memory footprint verification.

#### Feature 32: CLI ML Branching Strategy
- **Description**: Expose `--branching ml_gnn` achieving $\le 157$ nodes on `stein15.mps`.
- **Source**: ORIGINAL_REQUEST §R5, D-12
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F32-01`: Invoke `markov-cero-solve examples/stein15.mps --branching ml_gnn` and verify optimal solve.
  - `T1-F32-02`: Verify explored branch-and-bound nodes on `stein15.mps` is $\le 157$.
  - `T1-F32-03`: Compare node count of `--branching ml_gnn` vs `--branching pseudocost` on `stein15.mps`.
  - `T1-F32-04`: Verify CLI output JSON contains `"branching_strategy": "ml_gnn"`.
  - `T1-F32-05`: Verify `--branching ml_gnn` works on other MIPLIB easy instances (`flugpl`, `stein9`).
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F32-01`: Pass `--branching ml_gnn` on a pure LP problem; verify harmless fallback without error.
  - `T2-F32-02`: Fallback to pseudo-cost if ML scorer encounters numerical difficulty during search.
  - `T2-F32-03`: Combine `--branching ml_gnn` with parallel tree search (`--threads 4`).
  - `T2-F32-04`: Solve with node limit `--max-nodes 10` using `ml_gnn`; verify clean termination.
  - `T2-F32-05`: Invalid branching option passed (e.g. `--branching nonexistent`); verify CLI error message.

---

### Milestone 6: Full Datasets & Comparative Benchmark Harness (Features 33–38)

#### Feature 33: Netlib LP Full Suite Ingestion
- **Description**: Ingest all 97 standard Netlib LP instances with SHA-256 provenance JSONs.
- **Source**: ORIGINAL_REQUEST §R6, D-18
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F33-01`: Verify all 97 Netlib instances exist in `data/netlib/` or download script retrieves them.
  - `T1-F33-02`: Verify SHA-256 checksums match `data/netlib/provenance.json` for all 97 instances.
  - `T1-F33-03`: Parse 10 random Netlib instances via `mps.hpp` and verify row/col/nnz metrics.
  - `T1-F33-04`: Solve Netlib `afiro`, `adlittle`, `share2b` and confirm optimal objective matches ground truth.
  - `T1-F33-05`: Verify provenance JSON schema validates against `spec/provenance.schema.json`.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F33-01`: Corrupt file simulation: tamper with 1 byte in an MPS file and verify SHA-256 check fails.
  - `T2-F33-02`: Large Netlib instances (`fit2p`, `dfl001`) memory consumption during parsing.
  - `T2-F33-03`: Ingestion script execution under offline mode (skip cleanly if network unavailable).
  - `T2-F33-04`: Compressed files (.gz) extraction handling.
  - `T2-F33-05`: Read-only filesystem handling during ingestion verification.

#### Feature 34: MIPLIB 2017 Easy Suite Ingestion
- **Description**: Ingest designated MIPLIB 2017 easy benchmark instances with SHA-256 provenance.
- **Source**: ORIGINAL_REQUEST §R6, D-18
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F34-01`: Verify MIPLIB easy benchmark instances (`stein15`, `flugpl`, `blend2`, etc.) exist.
  - `T1-F34-02`: Verify SHA-256 checksums match `data/miplib/provenance.json`.
  - `T1-F34-03`: Verify integer variable count and constraint types match published MIPLIB statistics.
  - `T1-F34-04`: Solve `stein9` and `flugpl` to certified integer optimality.
  - `T1-F34-05`: Verify provenance JSON records origin URLs, timestamps, and file sizes.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F34-01`: Instance with semi-continuous or SOS variables; verify parser handling.
  - `T2-F34-02`: Verify skip behavior if optional large MIPLIB instances are omitted in shallow test runs.
  - `T2-F34-03`: Malformed MIPLIB line format handling.
  - `T2-F34-04`: Ingestion retry logic on intermittent download disconnects.
  - `T2-F34-05`: Duplicate instance name resolution in directory scanner.

#### Feature 35: Mittelmann Suite Ingestion
- **Description**: Ingest standard Mittelmann LP and MILP instances with provenance.
- **Source**: ORIGINAL_REQUEST §R6, D-18
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F35-01`: Verify Mittelmann benchmark dataset downloader script runs and fetches designated instances.
  - `T1-F35-02`: Verify SHA-256 provenance JSON for Mittelmann suite.
  - `T1-F35-03`: Parse Mittelmann LP instances and verify non-zero element count.
  - `T1-F35-04`: Verify ground truth reference tables for CPLEX, Gurobi, Xpress, HiGHS are parsed.
  - `T1-F35-05`: Execute solver on small Mittelmann instance and verify output logging.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F35-01`: Missing instance in Mittelmann suite; verify clean skip without test failure.
  - `T2-F35-02`: Mittelmann reference table format changes; test regex parser robustness.
  - `T2-F35-03`: Huge Mittelmann instances ($> 100,000$ rows) dimension validation.
  - `T2-F35-04`: Zero-byte downloaded file detection.
  - `T2-F35-05`: Special characters in Mittelmann instance filenames.

#### Feature 36: Convex QPLIB Ingestion
- **Description**: Ingest standard convex QPLIB instances with provenance JSONs.
- **Source**: ORIGINAL_REQUEST §R6, D-18
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F36-01`: Verify QPLIB downloader script fetches convex QPS files.
  - `T1-F36-02`: Verify SHA-256 provenance JSON for QPLIB instances.
  - `T1-F36-03`: Parse QPS file format with `QUADOBJ` section and verify matrix $P$ symmetry.
  - `T1-F36-04`: Verify convexity of ingested QPLIB models via eigenvalue / diagonal dominance check.
  - `T1-F36-05`: Solve convex QPLIB instance with ADMM and Active Set QP solvers.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F36-01`: QPLIB instance with indefinite $P$; verify solver correctly identifies non-convexity.
  - `T2-F36-02`: QPLIB instance with dense quadratic matrix $P$; verify memory allocation limit.
  - `T2-F36-03`: Linear objective missing in QPS (`q = 0`); verify pure quadratic solve.
  - `T2-F36-04`: Equality constrained QPLIB model.
  - `T2-F36-05`: QPLIB instance with unbounded objective direction.

#### Feature 37: Automated Multi-Solver Comparison
- **Description**: Author `scripts/run_full_compare.py` against HiGHS, GLPK, CBC, SCIP, & Mittelmann tables.
- **Source**: ORIGINAL_REQUEST §R6, D-19
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F37-01`: Run `scripts/run_full_compare.py --help` and verify all CLI options exist.
  - `T1-F37-02`: Run head-to-head comparison on 5 Netlib instances against HiGHS baseline.
  - `T1-F37-03`: Verify comparison script outputs structured CSV with status, objective, time, and ratio columns.
  - `T1-F37-04`: Verify geometric mean runtime calculation is computed correctly across instances.
  - `T1-F37-05`: Verify comparison harness tolerates solver timeout ($> 60$s) and assigns penalty time.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F37-01`: External solver binary (e.g. `glpsol`) missing on system; verify clean skip without crash.
  - `T2-F37-02`: Solver outputs disagree on objective beyond $10^{-4}$; verify discrepancy flag in CSV.
  - `T2-F37-03`: One solver reports optimal while another reports infeasible; verify contradiction flag.
  - `T2-F37-04`: Comparison on 0 shared instances; verify error handling.
  - `T2-F37-05`: Multi-threaded parallel instance execution within compare harness.

#### Feature 38: Dolan-Moré Performance Profiles
- **Description**: Generate `dolan_more_lp.svg` and `dolan_more_milp.svg` in `evidence/comparison/`.
- **Source**: ORIGINAL_REQUEST §R6, D-20
- **Tier 1 (Feature Coverage - 5 Tests)**:
  - `T1-F38-01`: Generate performance profile data curves $\rho_s(\tau)$ from benchmark CSV results.
  - `T1-F38-02`: Verify SVG file `dolan_more_lp.svg` is generated and is valid XML with SVG tags.
  - `T1-F38-03`: Verify SVG file `dolan_more_milp.svg` is generated and contains solver curve legends.
  - `T1-F38-04`: Verify performance ratio $\tau = 1.0$ matches the fraction of instances won by each solver.
  - `T1-F38-05`: Verify asymptote $\tau \to \infty$ equals the percentage of total instances solved.
- **Tier 2 (Boundary & Corner Cases - 5 Tests)**:
  - `T2-F38-01`: All solvers solve in identical time ($r_{s,p} = 1.0$); verify overlapping step functions.
  - `T2-F38-02`: One solver solves 0 instances; verify flat curve at $\rho = 0$.
  - `T2-F38-03`: Highly skewed runtimes ($10^5$ ratio range); verify logarithmic $\tau$ axis scaling.
  - `T2-F38-04`: Empty benchmark CSV input; verify graceful failure message.
  - `T2-F38-05`: SVG generation with custom styling (line thickness, color palette, dimensions).

---

### 5.2 Tier 3: Cross-Feature Combinations (Pairwise Coverage, 38 Tests)

Tier 3 tests verify that independently developed features operate seamlessly in combination across subsystem boundaries.

| Test ID | Primary Feature A | Interacting Feature B | Test Interaction Objective & Verification |
|---------|-------------------|-----------------------|-------------------------------------------|
| `T3-PAIR-01` | F01 (SparseLU IPM) | F06 (Iterative Refinement) | IPM normal equation solves ($A D A^T dy = rhs$) utilize always-on iterative refinement to keep barrier step residuals $< 10^{-14}$. |
| `T3-PAIR-02` | F01 (SparseLU IPM) | F07 (NumericalDiagnostic) | IPM encountering ill-conditioned normal equations emits structured diagnostic with condition estimate and failure site. |
| `T3-PAIR-03` | F02 (PDLP Stagnation) | F03 (PDLP Crossover) | Stagnation detection at iteration 1000 automatically triggers dual simplex crossover warm-start without user intervention. |
| `T3-PAIR-04` | F03 (PDLP Crossover) | F07 (NumericalDiagnostic) | Incomplete or failed crossover preserves interior iterates and populates diagnostic with primal/dual residuals. |
| `T3-PAIR-05` | F04 (ADMM Adaptive Rho) | F05 (ADMM Refactor Counter) | Adaptive $\rho$ adjustment correctly triggers KKT refactorization and increments `refactorization_count` by the exact update count. |
| `T3-PAIR-06` | F04 (ADMM Adaptive Rho) | F07 (NumericalDiagnostic) | ADMM QP solve records final primal and dual residuals in structured diagnostic on optimal termination. |
| `T3-PAIR-07` | F08 (Problem Classifier) | F09 (Engine Routing) | Classifier detects pure LP and router directs problem to `primal_simplex` or `pdlp` based on dimension and sparsity. |
| `T3-PAIR-08` | F08 (Problem Classifier) | F14 (SQP Solver) | Classifier detects continuous model with non-linear callbacks and routes directly to SQP engine. |
| `T3-PAIR-09` | F08 (Problem Classifier) | F18 (MINLP Outer Approx) | Classifier detects integer variables with non-linear callbacks and routes directly to MINLP Outer Approximation. |
| `T3-PAIR-10` | F08 (Problem Classifier) | F20 (NLOBJ Parser) | Parser detects `NLOBJ` section and classifier automatically tags model as `NLP`. |
| `T3-PAIR-11` | F10 (CUDA Broaden) | F11 (GPU Capability Guard) | Broadened architecture binary checks runtime GPU compute capability before kernel launch on target hardware. |
| `T3-PAIR-12` | F11 (GPU Capability Guard) | F12 (GPU ADMM Step) | GPU ADMM step kernel verifies capability guard before allocating device matrices; falls back to CPU if guard fails. |
| `T3-PAIR-13` | F12 (GPU ADMM Step) | F04 (ADMM Adaptive Rho) | GPU-accelerated matrix-vector multiplication operates seamlessly with host-side adaptive $\rho$ updates. |
| `T3-PAIR-14` | F13 (Crossover Study) | F03 (PDLP Crossover) | Empirical scale benchmark records crossover time from PDLP into dual simplex across scale points up to 5M nonzeros. |
| `T3-PAIR-15` | F14 (SQP Solver) | F15 (L-BFGS-B Hessian) | SQP solver utilizes L-BFGS-B compact two-loop recursion to approximate Lagrangian Hessian in quadratic subproblems. |
| `T3-PAIR-16` | F14 (SQP Solver) | F16 (Armijo-Wolfe Search) | SQP solver executes Armijo-Wolfe line search on $\ell_1$ merit function to select step length for iterate updates. |
| `T3-PAIR-17` | F14 (SQP Solver) | F17 (KKT Verifier) | SQP solver certifies converged point against independent KKT residual verifier with $\epsilon \le 10^{-6}$. |
| `T3-PAIR-18` | F18 (MINLP Outer Approx) | F14 (SQP Solver) | Outer approximation calls SQP solver to solve continuous NLP subproblems with fixed binary variables. |
| `T3-PAIR-19` | F18 (MINLP Outer Approx) | F07 (NumericalDiagnostic) | MINLP solve records subproblem condition numbers and MILP branch-and-cut metrics in structured diagnostic. |
| `T3-PAIR-20` | F19 (NlpModel API) | F14 (SQP Solver) | User-defined programmatic `NlpModel` with lambda callbacks is solved by SQP solver to high accuracy. |
| `T3-PAIR-21` | F20 (NLOBJ Parser) | F19 (NlpModel API) | MPS `NLOBJ` parser instantiates programmatic `NlpModel` with analytical gradient callbacks. |
| `T3-PAIR-22` | F21 (Python Module) | F22 (Python Bindings) | Sovereign CPython extension exposes `Model`, `NlpModel`, `SolveOptions`, and `solve()` cleanly to Python scripts. |
| `T3-PAIR-23` | F22 (Python Bindings) | F23 (Zero-Copy NumPy) | Python `Model` solution returns NumPy array memory view directly mapped to C++ solution vector. |
| `T3-PAIR-24` | F22 (Python Bindings) | F07 (NumericalDiagnostic) | Python `SolveResult` exposes `NumericalDiagnostic` fields as native Python dictionary or dataclass. |
| `T3-PAIR-25` | F24 (Pip Integration) | F21 (Python Module) | `pip install .` in clean environment compiles and installs `_core` CPython module passing smoke import. |
| `T3-PAIR-26` | F25 (Strong Branch Logger) | F26 (ML Data Partitioning) | Strong branching data logger outputs are partitioned strictly by instance into 70/15/15 disjoint splits. |
| `T3-PAIR-27` | F25 (Strong Branch Logger) | F27 (Bipartite Featurization) | Logger pairs exact branch scores with strict bipartite graph feature vectors for each candidate. |
| `T3-PAIR-28` | F26 (ML Data Partitioning) | F28 (Ranking Metrics) | Validation and test split evaluations compute Kendall's $\tau$ and NDCG@k strictly on out-of-sample instances. |
| `T3-PAIR-29` | F27 (Bipartite Featurization) | F29 (2-Layer Bipartite GCN) | Bipartite graph feature representations are fed directly into 2-layer GCN without feature dimension mismatch. |
| `T3-PAIR-30` | F29 (2-Layer Bipartite GCN) | F30 (Int8 ONNX Export) | PyTorch 2-layer GCN is calibrated and exported to Int8 quantized ONNX representation with verified weights. |
| `T3-PAIR-31` | F30 (Int8 ONNX Export) | F31 (C++ Inference) | Sovereign pure C++20 Int8 inference engine loads exported ONNX weights and reproduces Python GCN scores. |
| `T3-PAIR-32` | F31 (C++ Inference) | F32 (CLI ML Branching) | MILP branch-and-cut engine invokes C++ Int8 inference to score branching candidates when `--branching ml_gnn` is active. |
| `T3-PAIR-33` | F32 (CLI ML Branching) | F07 (NumericalDiagnostic) | MILP solve using ML branching records total inference time and node count in final `SolveResult`. |
| `T3-PAIR-34` | F33 (Netlib Ingestion) | F01 (SparseLU IPM) | Full Netlib LP suite instances are solved by SparseLU IPM and results recorded. |
| `T3-PAIR-35` | F34 (MIPLIB Ingestion) | F32 (CLI ML Branching) | Ingested MIPLIB easy instances are evaluated under `--branching ml_gnn` to verify node reduction. |
| `T3-PAIR-36` | F36 (QPLIB Ingestion) | F04 (ADMM Adaptive Rho) | Convex QPLIB instances are solved using ADMM QP solver with adaptive penalty parameter. |
| `T3-PAIR-37` | F37 (Multi-Solver Compare) | F33 (Netlib Ingestion) | Automated comparison script benchmarks markov-cero against HiGHS across ingested Netlib instances. |
| `T3-PAIR-38` | F38 (Dolan-Moré Profiles) | F37 (Multi-Solver Compare) | Comparison script CSV outputs are fed directly into Dolan-Moré performance profile SVG generator. |

---

### 5.3 Tier 4: Real-World Application Scenarios (19 Tests)

Tier 4 tests represent realistic, end-to-end mathematical programming formulations derived from industrial and operational domains.

| Test ID | Scenario Name | Formulation Characteristics & Mathematical Model | Verification Criteria |
|---------|---------------|--------------------------------------------------|-----------------------|
| `T4-APP-01` | Crude Oil Blending & Refining | Bilinear pooling formulation with component qualities (sulfur, octane, RVP) and volume balance. | Feasible blend yields certified profit; all specification bounds honored within $10^{-6}$. |
| `T4-APP-02` | Multi-Period Supply Chain Logistics | Dynamic multi-echelon network with warehouse capacities, transportation delays, and inventory holding costs. | Flow conservation holds at all nodes across all time periods; demand met exactly. |
| `T4-APP-03` | Power Dispatch (DC Optimal Power Flow) | Quadratic generator cost curves with linear PTDF line flow constraints and phase angle limits. | Generator outputs satisfy active power bounds; transmission lines within thermal limits. |
| `T4-APP-04` | Multi-Asset Portfolio Markowitz Optimization | Mean-variance quadratic program with $500 \times 500$ covariance matrix, sector limits, and long-only bounds. | Portfolio lies on efficient frontier; Sharpe ratio maximized; KKT residual $\le 10^{-6}$. |
| `T4-APP-05` | Chemical Process Network Synthesis | Mixed-integer linear program with reaction unit selection, feed splitters, and stoichiometric yields. | Optimal network topology selected; integer unit choices satisfy logical implications. |
| `T4-APP-06` | Multi-Facility Production Planning | Large MILP with setup integers, machine capacities, shift constraints, and overtime penalty costs. | Capacity constraints respected; MIP gap $\le 1\%$; setup costs correctly accrued. |
| `T4-APP-07` | Capacitated Vehicle Routing / Facility Location | Binary customer assignment with warehouse opening fixed charges and distance minimization. | Fixed-charge logical constraints enforced; vehicle load capacities never exceeded. |
| `T4-APP-08` | Unit Commitment in Power Systems | Multi-period mixed-integer commitment with minimum up/down times and startup/shutdown costs. | Inter-temporal generator state constraints satisfied; spinning reserves maintained. |
| `T4-APP-09` | Gas Pipeline Network Distribution | Non-linear Weymouth flow equations with pressure drops and compressor station energy consumption. | Non-linear pressure equations converge in SQP; mass flow continuity certified. |
| `T4-APP-10` | Telecom Bandwidth & Routing Optimization | Multicommodity network flow with link capacity bounds, packet QoS latency requirements, and rerouting. | No link congestion; total routing delay minimized; flow conservation satisfied. |
| `T4-APP-11` | Agricultural Resource & Crop Rotation | Multi-year planning with soil nitrogen depletion, water usage allocations, and price volatility. | Rotation sequencing constraints met; irrigation limit satisfied across seasons. |
| `T4-APP-12` | Aircraft Fleet Assignment & Routing | Binary tail assignment with airport slot restrictions, crew turnaround times, and maintenance cycles. | Every flight leg covered by exactly one aircraft; maintenance window intervals respected. |
| `T4-APP-13` | Financial Credit Risk Portfolio Minimization | Quadratic / semi-definite risk minimization under Value-at-Risk (VaR) regulatory constraints. | Portfolio risk minimized subject to target expected return and capital adequacy limits. |
| `T4-APP-14` | Supply Chain Safety Stock Optimization | Non-linear inventory holding cost under stochastic lead-time and Poisson demand arrivals. | SQP reaches certified optimal safety stock level satisfying target service level ($99\%$). |
| `T4-APP-15` | Industrial Cutting Stock & Bin Packing | Column generation / knapsack cutting patterns minimizing trim loss for raw paper/steel reels. | Trim waste minimized; integer demand for all cut widths fulfilled. |
| `T4-APP-16` | Energy Storage Battery Scheduling | Multi-period price arbitrage with battery round-trip efficiency, degradation costs, and SOC limits. | State-of-charge remains within $[20\%, 90\%]$; charging/discharging power ratings respected. |
| `T4-APP-17` | Municipal Water Distribution Pressure Control | Hydraulic network with non-linear Hazen-Williams friction head losses and pump performance curves. | Water demand satisfied at minimum pressure heads across all distribution junctions. |
| `T4-APP-18` | Job Shop Manufacturing Scheduling | Mixed-integer disjunctive programming with machine precedence constraints and makespan objective. | No overlapping operations on any single machine; makespan $C_{\text{max}}$ minimized. |
| `T4-APP-19` | Non-Convex Haverly Bilinear Pooling Benchmark | Classic non-convex chemical pooling problem (Haverly 1, 2, 3) solved via Outer Approximation / SLP. | Successfully reaches known global or certified local optimum with zero specification violations. |

---

## 6. Test Harness Architecture & Runner Design

### 6.1 Unified Execution Strategy
The test architecture uses a two-pronged execution mechanism:
1. **CTest Direct Execution**: Native CMake test targets (`add_test`) compiled into standalone C++ binaries in `tests/e2e/`. These participate in standard `ctest` runs and GitHub Actions CI.
2. **Sovereign Python Test Runner (`scripts/run_e2e_tests.py`)**: An intelligent test orchestrator capable of filtering tests by tier, milestone, or feature; executing benchmarks; capturing performance profiles; and producing structured JUnit XML and JSON summary reports.

```
       Developer / CI / Automated Orchestrator
                         |
           +-------------+-------------+
           |                           |
  ctest --output-on-failure   python3 scripts/run_e2e_tests.py
           |                     [--tier 1,2,3,4]
           |                     [--milestone M1..M6]
           |                     [--feature 1..38]
           |                     [--json report.json]
           |                     [--xml junit.xml]
           v                           v
+-------------------------------------------------------------+
|                     CMake Test Targets                      |
|  - e2e_tier1_m1_features    - e2e_tier3_m1_combinations     |
|  - e2e_tier2_m1_boundaries  - e2e_tier4_m1_scenarios        |
+-------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                  markov_cero Core Library                   |
|  (SparseLU, PDLP, ADMM, SQP, MINLP, B&B, ML, Classifiers)    |
+-------------------------------------------------------------+
```

### 6.2 Test Source Directory Layout
All E2E test suites reside in `tests/e2e/`:
```
tests/e2e/
├── e2e_test_framework.hpp         # Sovereign test macros, assertions, fixture helpers
├── test_tier1_m1_features.cpp     # Tier 1 tests for Milestone 1 (Features 1 to 7)
├── test_tier2_m1_boundaries.cpp   # Tier 2 tests for Milestone 1 (Boundaries & corners)
├── test_tier3_m1_combinations.cpp # Tier 3 tests for Milestone 1 (Pairwise cross-features)
└── test_tier4_m1_scenarios.cpp    # Tier 4 tests for Milestone 1 (Domain applications)
```

### 6.3 Sovereign Test Framework (`e2e_test_framework.hpp`)
In accordance with clean-room sovereignty, no third-party test libraries are used. The framework provides:
- **`TEST_CASE(name)`**: Macro defining isolated test functions with automatic exception catching and timing.
- **`E2E_ASSERT(condition, message)`**: Strict boolean assertion emitting file and line info on failure.
- **`E2E_ASSERT_NEAR(a, b, tol, message)`**: Floating-point proximity assertion checking $|a - b| \le \text{tol}$.
- **`E2E_ASSERT_KKT(primal_res, dual_res, tol)`**: Specialized KKT residual validation check.
- **`TestRegistry`**: Thread-safe test case registration and execution registry supporting test filtering by tier and milestone.

### 6.4 Sovereign Python Test Runner (`scripts/run_e2e_tests.py`)
Features of the test runner script:
- Zero external Python dependencies (uses `sys`, `os`, `json`, `subprocess`, `argparse`, `pathlib`, `xml.etree.ElementTree`).
- Supports arguments:
  - `--tier <1|2|3|4|all>`: Filter execution to specific test tier(s).
  - `--milestone <M1|M2|M3|M4|M5|M6|all>`: Filter execution to specific milestone(s).
  - `--feature <1..38>`: Execute only tests addressing a specific feature ID.
  - `--json <path>`: Emit machine-readable JSON execution log with timings and residuals.
  - `--xml <path>`: Emit standard JUnit XML report for CI integration.
  - `--list`: List all available tests in the feature inventory.
  - `--verbose`: Enable detailed per-test output.

---

## 7. Pass / Fail Criteria & Acceptance Gates

A test run is certified as **PASSED** if and only if all of the following conditions are met:
1. **Zero Test Failures**: 100% of executed test cases exit with code 0 and all assertions evaluate to true.
2. **Zero Regressions**: All 59 baseline CTest targets continue to pass without degradation.
3. **Certified KKT Tolerances**:
   - Continuous LP (Simplex/IPM): Primal residual $\le 10^{-7}$, Dual residual $\le 10^{-7}$, Complementarity $\le 10^{-7}$.
   - First-Order LP (PDLP before crossover): Residuals $\le 10^{-4}$.
   - PDLP after crossover: Certified vertex basis with KKT residuals $\le 10^{-7}$.
   - Convex QP (ADMM / Active Set): Primal residual $\le 10^{-4}$, Dual residual $\le 10^{-4}$.
   - NLP (SQP): KKT residual $\le 10^{-6}$.
   - MINLP: Integer feasibility violation $\le 10^{-6}$, relative optimality gap $\le 10^{-4}$.
4. **Clean-Room Sovereignty Compliance**: `scripts/check-sovereignty.py` returns 0 violations on all binaries.
5. **Deterministic Diagnostics**: Every solve result populates `NumericalDiagnostic` with valid floating-point numbers (no uninitialized memory or NaNs).

---

## 8. Summary of Milestones & Progressive Rollout

| Milestone | Features In Scope | Test Tiers Active | Target Test Target Executable |
|-----------|-------------------|-------------------|--------------------------------|
| **Milestone 1** | Features 1–7 (SparseLU, PDLP Crossover, ADMM $\rho$, Refinement, Diagnostics) | Tier 1, 2, 3, 4 (M1 subset) | `e2e_tier1_m1_features`, `e2e_tier2_m1_boundaries`, etc. |
| **Milestone 2** | Features 8–13 (Classifier, CUDA Broaden, GPU ADMM Step, Crossover Study) | Tier 1, 2, 3, 4 (M2 subset) | `e2e_tier1_m2_features`, `e2e_tier2_m2_boundaries`, etc. |
| **Milestone 3** | Features 14–20 (SQP, L-BFGS, Merit Search, KKT Verifier, MINLP, NLOBJ) | Tier 1, 2, 3, 4 (M3 subset) | `e2e_tier1_m3_features`, `e2e_tier2_m3_boundaries`, etc. |
| **Milestone 4** | Features 21–24 (Python Module, Model Bindings, Zero-Copy NumPy, Pip) | Tier 1, 2, 3, 4 (M4 subset) | `e2e_python_suites` (pytest + CTest) |
| **Milestone 5** | Features 25–32 (ML Logger, 70/15/15 Split, Bipartite GCN, Int8 ONNX, C++ Inference) | Tier 1, 2, 3, 4 (M5 subset) | `e2e_tier1_m5_features`, `e2e_ml_branching_test` |
| **Milestone 6** | Features 33–38 (Netlib 97, MIPLIB Easy, Mittelmann, QPLIB, Comparison, Dolan-Moré) | Tier 1, 2, 3, 4 (M6 subset) | `e2e_benchmark_full_suite`, `compare_harness` |
| **Final** | Full Suite Integration & Adversarial Hardening | All Tiers (100% Pass) | Full `scripts/run_e2e_tests.py --all` |

---
*End of TEST_INFRA.md*
