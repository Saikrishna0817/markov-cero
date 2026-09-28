# markov-cero — Comprehensive Specification Mining Survey Report

**Document ID:** SPEC-MINER-SURVEY-2026-09-27  
**Agent:** spec_miner_1 (`teamwork_preview_spec_miner`)  
**Workspace:** `/home/saikrishna/markov-initial-build`  
**Working Directory:** `/home/saikrishna/markov-initial-build/.agents/spec_miner_survey_1`  
**Date:** 2026-09-27  
**Version Target:** v0.6.0+ (Milestones 1–6, Workstreams W1–W9)  

---

## 1. Executive Summary & Authoritative Specification Sources

This specification mining survey aggregates, reconciles, and formalizes all functional, numerical, mathematical, architectural, and evaluation requirements across the authoritative materials:
1. **User Request & Requirements:** `/home/saikrishna/markov-initial-build/.agents/ORIGINAL_REQUEST.md`
2. **Authoritative Implementation Plan:** `/home/saikrishna/.gemini/antigravity/brain/e5ad74fb-1631-4c2d-9b6d-27959c134ad2/implementation_plan.md` (and `docs/audit/implementation_plan.md`)
3. **Ground Truth & Gap Analysis:** `docs/audit/00-ground-truth.md`
4. **Research → Code Traceability Matrix:** `docs/audit/21-traceability.md`
5. **Post-SIH Architecture:** `docs/audit/22-post-sih-architecture.md`
6. **Numerical Robustness Dossier:** `evidence/robustness_dossier.md`
7. **Existing C++20 Core Codebase:** `src/`, `include/markov_cero/`, `gpu/`, `apps/`, `tests/`

### 1.1 Invariant Architectural Constraints (C1–C6)
All 9 workstreams and 6 milestones are governed by six non-negotiable repository invariants:
- **C1 — Sovereignty:** Zero linking of any third-party optimization solver library (no HiGHS, GLPK, CBC, SCIP, CPLEX, Gurobi linked into binaries). `pybind11` and embedded ONNX headers are permitted strictly as build-time or inference runtimes, not solver engines.
- **C2 — Clean-Room:** All mathematical algorithms implemented from first principles. No decompilation, porting, or copy-pasting of foreign solver code.
- **C3 — Zero Silent Failures:** Every numerical failure or premature abort must return a structured `NumericalDiagnostic` containing primal/dual residuals, condition estimate, failure site, and recovery recommendations.
- **C4 — Zero-Trust Verification:** No solution may be reported as `Optimal` without independent verification through `primal_verifier` and KKT residual validation ($\epsilon \le 10^{-6}$ for LP/QP/NLP).
- **C5 — Single Static Library:** All core C++ code compiles into `markov_cero_core`. No multiple dynamic libraries.
- **C6 — Single Root CMakeLists.txt:** All build targets, tests, and options must be defined in the root `/home/saikrishna/markov-initial-build/CMakeLists.txt`. No subdirectory CMakeLists.

### 1.2 Locked Decisions Summary (D-01 through D-20)
| ID | Domain | Locked Decision |
|---|---|---|
| D-01 | NLP Inputs | Dual modalities: Programmatic C++/Python callback API (`NlpModel`) AND file-based MPS extension (`NLOBJ` section). |
| D-02 | NLP Algorithm | Sequential Quadratic Programming (SQP) with L-BFGS-B Hessian approximation (memory $m=10$), Armijo-Wolfe line search on $\ell_1$ merit function, QP subproblem via ADMM. |
| D-03 | MINLP Algorithm | Outer Approximation (LP/NLP Branch-and-Bound, Duran & Grossmann 1986; Bonami et al. 2008), strictly convex MINLP only. Non-convex MINLP is post-plan. |
| D-04 | ML Branching Scope | Fully implemented for SIH submission; offline-trained 2-layer bipartite GCN, Int8 ONNX embedded inference. |
| D-05 | ML Data Source | Self-generated from Markov-CERO running strong branching on MIPLIB 2017 easy instances; zero external model weights. |
| D-06 | GPU Hardware Target | All NVIDIA architectures $\ge \text{sm\_50}$ (Maxwell through Hopper/Blackwell). AMD HIP / Intel SYCL documented as planned extension points. |
| D-07 | CUDA Compilation | `CMAKE_CUDA_ARCHITECTURES = "all-major"` (compiles sm_50, 60, 70, 75, 80, 86, 89, 90 + PTX JIT). |
| D-08 | GPU Engine Scope | Strictly limited to SpMV-dominated paths: Large LP via PDLP ($\text{NNZ} > 500k$) and Large QP via ADMM ($\text{NNZ}(P) > 100k$). No GPU for Simplex, MILP B&B, NLP, or MINLP. |
| D-09 | Commercial Benchmarks | Mittelmann published benchmark tables (plato.asu.edu/bench.html) for CPLEX, Gurobi, Xpress; no commercial licenses required. |
| D-10 | Free Solvers Comparison | Local installation and automated benchmarking against HiGHS, GLPK, COIN-OR CBC, and SCIP. |
| D-11 | Python Bindings | `pybind11` header-only binding library generating CPython `.so` with zero runtime dependency. |
| D-12 | NLP File Format | Custom `NLOBJ` / `NLCON` MPS extension format; reject AMPL `.nl` due to external parser complexity. |
| D-13 | Sovereignty Guard | Zero external solver libraries; `scripts/check-sovereignty.py` enforced in CI. |
| D-14 | Sparse IPM Scaling | Replace dense LU normal equations factorizer in `src/lp/interior/ipm.cpp` with sparse Cholesky / `SparseLU` normal equations solver ($A D A^T$). |
| D-15 | PDLP Crossover | Stagnation detector (window 1000, threshold 0.999) triggering candidate basis extraction and warm-start into dual simplex. |
| D-16 | ADMM Penalty $\rho$ | Adaptive update rule (Boyd et al. 2011) with $\rho \in [10^{-6}, 10^6]$, factorizing KKT on update. |
| D-17 | Iterative Refinement | Always-on one pass in `src/linalg/sparse_basis.cpp` with 80-bit `long double` residual computation and early exit at $10^{-14}$. |
| D-18 | ML Inference Runtime | Embedded header-only ONNX runtime (`ortinference.hpp`), compiled conditionally under `-DMARKOV_CERO_ENABLE_ML=ON`. |
| D-19 | Dataset Policy | Large datasets gitignored in `data/<suite>_full/`; downloaded on-demand with SHA-256 provenance JSON files committed to git. |
| D-20 | NLOBJ Specification | Documented in `docs/nlobj_format.md`; supports polynomial monomials up to degree 2. |

---

## 2. Comprehensive Workstream & Milestone Specifications

### 2.1 Milestone 1: Numerical Accuracy Hardening (W5)

#### 2.1.1 Sparse Normal Equations IPM (`src/lp/interior/ipm.cpp`, `src/linalg/sparse_basis.cpp`)
- **Current Limitation:** In `ipm.cpp`, Newton direction normal equations $(A D A^T) \Delta y = \text{rhs}$ forms an $m \times m$ dense matrix `std::vector<double> d_row_major(m * m, 0.0)` in an $O(m^2 n)$ loop and calls `linalg::DenseLu::factorize()`. Memory explodes at $m \ge 200$, failing Netlib instances like `sc205`, `share1b`.
- **Mathematical Formulation:**
  For standard canonical form:
  $$\min c^T x \quad \text{s.t.} \quad A x = b, \ x \ge 0$$
  The perturbed KKT conditions are:
  $$A x = b, \quad A^T y + s = c, \quad X S e = \mu e, \quad (x, s) > 0$$
  The Newton step system:
  $$\begin{bmatrix} 0 & A^T & I \\ A & 0 & 0 \\ S & 0 & X \end{bmatrix} \begin{bmatrix} \Delta x \\ \Delta y \\ \Delta s \end{bmatrix} = \begin{bmatrix} -r_d \\ -r_p \\ \tau - X S e \end{bmatrix}$$
  Eliminating $\Delta s = -r_d - A^T \Delta y$ and $\Delta x = S^{-1}(\tau - X S e - X \Delta s)$ yields the normal equations:
  $$(A D A^T) \Delta y = -r_p - A u$$
  where $D = \text{diag}(x / s)$ with clamping $D_{jj} = \text{clamp}(x_j / s_j, 10^{-12}, 10^{12})$, and $u_j = \frac{\tau_j}{s_j} - x_j + \frac{x_j}{s_j} r_{d,j}$.
- **Algorithmic Specification:**
  1. Build sparse $M = A D A^T$ directly as a `SparseCsc` matrix without forming any dense $m \times m$ buffer.
     Let column $j$ of $A$ have nonzeros in row set $\mathcal{R}_j$. Then:
     $$M = \sum_{j=1}^n D_{jj} A_{*,j} A_{*,j}^T$$
     Accumulate nonzeros $(i, k)$ for $i, k \in \mathcal{R}_j$ using sparse hash/triplet accumulation, then convert to CSC.
  2. Factorize $M$ using `SparseLu::factorize(normal_sparse, kMarkowitzThreshold, max_nonzeros, true)` with minimum-degree column pre-ordering and Markowitz threshold pivoting ($u = 0.1$).
  3. Escalation fallback: If numerical singularity occurs, add dynamic diagonal regularization:
     $$M_{\text{reg}} = M + \delta \max_i(M_{ii}) I$$
     with $\delta \in [10^{-10}, 10^{-2}]$ scaling geometrically by $100\times$.
  4. Solve for $\Delta y$ via sparse forward and back-substitution.
  5. Back-substitute:
     $$\Delta s = -A^T \Delta y - r_d$$
     $$\Delta x_j = \frac{\tau_j}{s_j} - x_j - \frac{x_j}{s_j} \Delta s_j$$
- **Target Scale:** Scales from $m \approx 200$ to $m \ge 50,000$ rows.

#### 2.1.2 PDLP Stagnation Detection & Crossover (`src/lp/first_order/pdlp.cpp`)
- **Current Limitation:** PDLP runs Chambolle-Pock first-order iterations. On ill-conditioned Netlib instances (`kb2`, `lotfi`, `beaconfd`), the primal/dual residuals stall near $10^{-4}$ to $10^{-5}$, failing to reach high precision ($10^{-7}$) before hitting iteration limits.
- **Mathematical Formulation & Algorithm:**
  1. **Stagnation Detector Struct:**
     ```cpp
     struct StagnationDetector {
         static constexpr std::size_t kWindow = 1000;
         static constexpr double kImprovementThreshold = 0.999; // < 0.1% improvement
         double best_residual = std::numeric_limits<double>::infinity();
         std::size_t stagnant_count = 0;
         bool check(double residual) {
             if (residual < best_residual * kImprovementThreshold) {
                 best_residual = residual;
                 stagnant_count = 0;
                 return false;
             }
             return ++stagnant_count >= kWindow;
         }
     };
     ```
  2. **Candidate Basis Extraction:**
     Given PDLP iterate $x^*$, classify variables into candidate basic/nonbasic sets:
     - Nonbasic at lower bound: $x_j^* \approx l_j$ (i.e. $|x_j^* - l_j| \le \epsilon_{\text{tol}}$).
     - Nonbasic at upper bound: $x_j^* \approx u_j$.
     - Basic candidates: $l_j + \epsilon_{\text{tol}} < x_j^* < u_j - \epsilon_{\text{tol}}$.
     Sort candidate basic variables by distance from bounds $\min(x_j^* - l_j, u_j - x_j^*)$ descending.
     Perform rank-revealing echelon insertion to select $m$ linearly independent columns. Fill deficient rank with slack/artificial columns.
  3. **Dual Simplex Warm-Start Crossover:**
     Instantiate `lp::dual::DualSimplexEngine` with extracted basis state $\mathcal{B}$.
     Run dual simplex Phase II with Harris two-pass ratio test and Forrest-Goldfarb steepest-edge pricing to reach exact vertex KKT $\le 10^{-7}$.
  4. **Singular Basis Catch:**
     If the extracted basis factorization fails (singular or condition estimate $> 10^{14}$), catch exception, abort crossover gracefully, and return PDLP solution with `convergence_note: "stagnated at tolerance floor"`. Never crash.

#### 2.1.3 ADMM Adaptive $\rho$ Scheme (`src/qp/admm_solver.cpp`)
- **Current Limitation:** `options_.rho_init = 1.0` remains fixed for the entire solve. On poorly scaled QPs, either primal or dual residuals converge orders of magnitude slower.
- **Mathematical Formulation (Boyd et al. 2011, Section 3.4.1):**
  At each ADMM iteration, compute primal residual $r_{\text{prim}}^{k+1} = A x^{k+1} - z^{k+1}$ and dual residual $r_{\text{dual}}^{k+1} = \rho A^T (z^{k+1} - z^k)$.
  Let $\mu = 10.0$, $\tau_{\text{incr}} = 2.0$, $\tau_{\text{decr}} = 2.0$, $\rho_{\min} = 10^{-6}$, $\rho_{\max} = 10^6$.
  Update penalty parameter $\rho$:
  $$\rho^{k+1} = \begin{cases} \rho^k \cdot \tau_{\text{incr}}, & \text{if } \|r_{\text{prim}}^k\|_\infty > \mu \|r_{\text{dual}}^k\|_\infty \text{ and } \rho^k < \rho_{\max} \\ \rho^k / \tau_{\text{decr}}, & \text{if } \|r_{\text{dual}}^k\|_\infty > \mu \|r_{\text{prim}}^k\|_\infty \text{ and } \rho^k > \rho_{\min} \\ \rho^k, & \text{otherwise} \end{cases}$$
- **KKT Matrix Refactorization:**
  When $\rho$ is modified, re-factorize the quasi-definite KKT system:
  $$K = \begin{bmatrix} P + \sigma I & A^T \\ A & -\rho^{-1} I \end{bmatrix}$$
  using LDLT factorization (`src/qp/kkt.cpp`).
  Track and emit refactorization count: `"admm_rho_updates": N` in JSON diagnostics.

#### 2.1.4 Always-On Extended-Precision Iterative Refinement (`src/linalg/sparse_basis.cpp`)
- **Current Limitation:** Refinement in `SparseBasisFactorization::solve()` is gated by `if (refinement_required())` which only triggered if condition proxy $> 10^8$ or updates $\ge 50$. Moderately ill-conditioned systems ($\kappa \approx 10^5 - 10^7$) suffered forward error degradation.
- **Mathematical Formulation & Algorithmic Update (Decision D-17):**
  Always execute one pass of iterative refinement on every basis solve:
  1. Compute initial solve $x = B^{-1} b$.
  2. Compute exact residual $r = b - B x$ using IEEE 754 80-bit `long double` accumulators:
     $$r_i = \text{fl}_{80}\left(b_i - \sum_{j} B_{ij} x_j\right)$$
  3. Check residual norm $\|r\|_\infty$. If $\|r\|_\infty < 10^{-14}$, exit immediately (sub-0.05ms overhead).
  4. If $\|r\|_\infty \ge 10^{-14}$, solve error correction system:
     $$B \Delta x = r$$
  5. Apply correction: $x \leftarrow x + \Delta x$.
  Ensures forward error $< 10^{-6}$ and eliminates silent accuracy loss on barely ill-conditioned bases.

#### 2.1.5 Structured Numerical Diagnostics Protocol (`include/markov_cero/api/solve.hpp`, `src/api/api.cpp`)
- **Enforcement (Invariant C3):**
  Eliminate all silent exits, raw returns, and unexplained `NumericalFailure` states across all engines (Simplex, IPM, PDLP, MILP, QP, SQP).
- **C++ Data Structure:**
  ```cpp
  struct NumericalDiagnostic {
      double primal_residual{0.0};        // ||Ax - b||_inf at failure time
      double dual_residual{0.0};          // ||A^T y + s - c||_inf
      double complementarity_gap{0.0};    // |c^T x - b^T y|
      double condition_estimate{0.0};     // kappa_hat(B) or max|U_ii| / min|U_ii|, else NaN
      std::string failure_location;      // e.g. "sparse_basis:factor (pivot < 1e-12)"
      std::string suggested_action;      // e.g. "try --ruiz-iterations 20 --no-presolve"
  };
  ```
  Added to `SolveResult`: `std::optional<NumericalDiagnostic> numerical_diagnostic;`.
- **JSON Output Serialization:**
  When `status != SolveStatus::optimal`, the JSON report MUST contain the `numerical_diagnostic` block with actionable guidance.

---

### 2.2 Milestone 2: Problem Classification & GPU Polish (W6 + W3)

#### 2.2.1 Sovereign Model Classifier (`src/model/classifier.cpp`, `include/markov_cero/model/classifier.hpp`)
- **Problem Class Hierarchy:**
  `enum class ProblemClass { LP, MILP, QP, MIQP, NLP, MINLP };`
- **Authoritative Decision Tree:**
  ```
  1. Are NLP callbacks present in NlpModel (non-null f, grad_f, g, or J_g)?
     ├── YES ──> Has integer variables?
     │            ├── YES ──> MINLP
     │            └── NO  ──> NLP
     └── NO  ──>
  2. Does the model contain an NLOBJ section in MPS?
     ├── YES ──> Has integer variables?
     │            ├── YES ──> MINLP
     │            └── NO  ──> NLP
     └── NO  ──>
  3. Does the model contain quadratic objective terms (QUADOBJ/QMATRIX or non-empty P)?
     ├── YES ──> Has integer variables?
     │            ├── YES ──> MIQP
     │            └── NO  ──> QP
     └── NO  ──>
  4. Does the model contain integer or binary variables (VariableType != continuous)?
     ├── YES ──> MILP
     └── NO  ──> LP
  ```
- **Authoritative Engine Auto-Selection Table & Thresholds:**
  | Resolved Class | Default Engine | Condition Override | Rationale |
  |---|---|---|---|
  | **LP** | `primal` (simplex) | If $\text{NNZ}(A) > 50,000 \implies$ `pdlp` | Simplex is faster for small/medium; first-order PDLP dominates on ultra-sparse scale |
  | **LP (PDLP)** | `pdlp --backend cpu` | If `--backend gpu` AND $\text{NNZ}(A) > 500,000 \implies$ GPU | GPU kernel launch and PCIe transfer cost requires $\text{NNZ} > 500k$ to achieve net speedup |
  | **MILP** | `milp` (single thread) | If `--threads > 1 \implies$ `parallel` | Multithreaded tree search with work stealing |
  | **QP** | `qp` (ADMM CPU) | If $\text{NNZ}(P) > 100,000$ AND `--backend gpu` $\implies$ GPU ADMM | SpMV for $P x$ offloaded to CUDA tensor/CUDA cores |
  | **MIQP** | `miqp` | Always | Branch-and-bound with continuous QP node relaxations |
  | **NLP** | `sqp` | Always | Sequential Quadratic Programming with L-BFGS-B |
  | **MINLP** | `outer_approx` | Always | Outer Approximation with SQP subproblems and MILP master |

#### 2.2.2 GPU Infrastructure & CUDA "all-major" Compilation
- **CMake Architecture Configuration (`CMakeLists.txt`):**
  Change `CMAKE_CUDA_ARCHITECTURES` from `"75;80;86;89;90"` to `"all-major"`.
  Generates binary PTX and SASS covering:
  - sm_50 (Maxwell: GTX 750 Ti, 900 series)
  - sm_60 (Pascal: P100, GTX 1080)
  - sm_70 (Volta: V100)
  - sm_75 (Turing: T4, RTX 2000 series)
  - sm_80 / sm_86 (Ampere: A100, RTX 3000 series)
  - sm_89 (Ada Lovelace: L40, RTX 4000 series)
  - sm_90 (Hopper: H100)
  - PTX JIT fallback for future architectures (Blackwell sm_100, etc.)
- **Runtime Compute Capability Verification (`gpu/src/device.cpp`):**
  Inspect `props.major` at runtime. If `props.major < 5`:
  Emit warning: `"[gpu] WARNING: device sm_%d%d is below the supported sm_50 minimum. Falling back to CPU PDLP.\n"`.
  Return `false` from `is_gpu_available()` to trigger transparent, non-crashing CPU fallback.

#### 2.2.3 GPU-Accelerated QP ADMM Step Kernel (`gpu/kernels/admm_step.cu`, `gpu/src/admm_matvec.cpp`)
- **Kernel Operation:**
  For large QP instances ($\text{NNZ}(P) > 100,000$), offload the $x$-update step to GPU:
  $$v = P x + \rho x$$
  followed by elementwise box projection:
  $$x_j^{k+1} = \text{clamp}(v_j, l_j, u_j)$$
- **Memory Layout:** $P$ stored in CSR format on device memory; vectors $x, q, z, y$ mapped via device buffers (`GpuBuffer<double>`).
- **CPU Fallback (`gpu/src/admm_matvec.cpp`):** Performs identical operation using multithreaded OpenMP/CPU sparse CSR loops when GPU is disabled or unavailable.

#### 2.2.4 Large-Scale Crossover Empirical Study (`scripts/generate_large_scale_study.py`)
- **Instance Generation:** Generate synthetic sparse models with Netlib/industrial topology at scales:
  - `scale_100k.mps` ($10^5$ nonzeros)
  - `scale_500k.mps` ($5 \times 10^5$ nonzeros)
  - `scale_1m.mps` ($10^6$ nonzeros)
  - `scale_5m.mps` ($5 \times 10^6$ nonzeros)
- **Benchmarking Protocol:** Benchmark end-to-end solve time (including PCIe data transfer H2D/D2H) comparing CPU PDLP vs GPU PDLP. Document precise empirical crossover point in `evidence/benchmarks/crossover_study.csv`.

---

### 2.3 Milestone 3: Nonlinear & Mixed-Integer Nonlinear Programming (W1)

#### 2.3.1 Dual Input Modalities (Programmatic Callback & File-Based NLOBJ)
- **Path A — C++ / Python Callback API (`NlpModel`):**
  User defines function objects:
  - Objective: $f(x): \mathbb{R}^n \to \mathbb{R}$
  - Objective gradient: $\nabla f(x): \mathbb{R}^n \to \mathbb{R}^n$
  - Inequality constraints: $g(x): \mathbb{R}^n \to \mathbb{R}^{m_{ineq}}$
  - Inequality Jacobian: $J_g(x): \mathbb{R}^n \to \mathbb{R}^{m_{ineq} \times n}$
  - Equality constraints: $h(x): \mathbb{R}^n \to \mathbb{R}^{m_{eq}}$
  - Equality Jacobian: $J_h(x): \mathbb{R}^n \to \mathbb{R}^{m_{eq} \times n}$
  - Variable bounds: $l \le x \le u$
- **Path B — Custom MPS `NLOBJ` / `NLCON` Section Parser (`src/io/nlobj_parser.cpp`):**
  File format specification (`docs/nlobj_format.md`):
  Extends standard MPS to express quadratic and multilinear polynomial terms without requiring AMPL `.nl` binaries:
  ```
  NLOBJ
  * Format: COEFF  VAR1  [VAR2]
    0.5   x1    x1          (signifies 0.5 * x1^2)
    -1.0  x1    x2          (signifies -1.0 * x1 * x2)
  NLCON
    g1 <= 0    x1 * x1 + x2 - 4.0
  ENDATA
  ```
  Transcendental functions ($\sin, \exp, \log$) prompt the user to use the programmatic callback API.

#### 2.3.2 Sequential Quadratic Programming (SQP) Solver (`src/nlp/sqp_solver.cpp`)
- **Mathematical Formulation:**
  Consider general nonlinear optimization:
  $$\min_{x} f(x) \quad \text{s.t.} \quad g(x) \le 0, \quad h(x) = 0, \quad l \le x \le u$$
  At iteration $k$, formulate the quadratic programming subproblem:
  $$\min_{d} \nabla f(x^k)^T d + \frac{1}{2} d^T B_k d$$
  $$\text{s.t.} \quad g(x^k) + \nabla g(x^k)^T d \le 0$$
  $$h(x^k) + \nabla h(x^k)^T d = 0$$
  $$l - x^k \le d \le u - x^k$$
  where $B_k$ is the positive-definite L-BFGS-B Hessian approximation.
- **L-BFGS-B Quasi-Newton Approximation (`src/nlp/lbfgs.cpp`):**
  Maintain memory window of $m = 10$ correction pairs:
  $$s_k = x_{k+1} - x_k, \quad y_k = \nabla_x \mathcal{L}(x_{k+1}, \lambda_{k+1}, \nu_{k+1}) - \nabla_x \mathcal{L}(x_k, \lambda_{k+1}, \nu_{k+1})$$
  Enforce Powell damping to ensure positive definiteness $s_k^T y_k \ge 0.2 s_k^T B_k s_k$.
  Evaluate two-loop recursion to compute $H_k \nabla f(x^k)$.
- **QP Subproblem Solution:**
  Solved using Markov-CERO's internal `qp::admm_solver`.
- **Line Search on $\ell_1$ Merit Function (`src/nlp/merit_function.cpp`):**
  $$\phi(x; \mu) = f(x) + \mu \left( \sum_{i=1}^{m_{ineq}} \max(0, g_i(x)) + \sum_{j=1}^{m_{eq}} |h_j(x)| \right)$$
  Penalty parameter update: $\mu > \|\lambda\|_\infty + \|\nu\|_\infty$.
  Armijo condition: $\phi(x^k + \alpha d; \mu) \le \phi(x^k; \mu) + c_1 \alpha D \phi(x^k; d)$, with $c_1 = 10^{-4}$.
  Curvature / Strong Wolfe condition: with $c_2 = 0.9$.
- **KKT Residual Verifier (`src/nlp/nlp_verifier.cpp`):**
  Evaluate infinity norm of stationarity, primal feasibility, and complementary slackness:
  $$\|r_{\text{KKT}}\|_\infty = \max\left( \|\nabla f(x) + J_g(x)^T \lambda + J_h(x)^T \nu\|_\infty, \ \|(g(x))^+\|_\infty, \ \|h(x)\|_\infty, \ \|\lambda \odot g(x)\|_\infty \right) \le 10^{-6}$$
- **Numerical Safeguards & Fallbacks:**
  If the QP subproblem fails or $B_k$ becomes indefinite, reset $B_k$ to the scaled identity $B_k = \gamma I$ and restart from the current iterate.
  Maximum 3 resets permitted; if convergence stalls, exit with structured `NumericalDiagnostic`.

#### 2.3.3 Convex MINLP Outer Approximation (`src/minlp/minlp_solver.cpp`, `src/minlp/outer_approx.cpp`)
- **Mathematical Derivation (Duran & Grossmann 1986; Bonami et al. 2008):**
  For mixed-integer convex nonlinear programs:
  $$\min_{x, y} f(x, y) \quad \text{s.t.} \quad g(x, y) \le 0, \quad x \in \mathbb{R}^n, \quad y \in \mathbb{Z}^p$$
- **Algorithm Flow:**
  1. Solve continuous NLP relaxation (relaxing $y \in \mathbb{Z}^p \to y \in \mathbb{R}^p$) via SQP solver to obtain initial point $(x^0, y^0)$.
  2. Generate first-order outer approximation supporting hyperplanes at solution $(x^k, y^k)$:
     $$f(x^k, y^k) + \nabla f(x^k, y^k)^T \begin{bmatrix} x - x^k \\ y - y^k \end{bmatrix} \le \alpha$$
     $$g_i(x^k, y^k) + \nabla g_i(x^k, y^k)^T \begin{bmatrix} x - x^k \\ y - y^k \end{bmatrix} \le 0, \quad \forall i$$
  3. Master MILP Problem: Solve the linearized master problem with integer constraints using `milp::solve_milp()`:
     $$\min \alpha \quad \text{s.t.} \quad \text{linearization cuts}, \quad y \in \mathbb{Z}^p$$
  4. Fix integer variables $y = y^{k+1}$ from master MILP solution. Solve primal NLP subproblem over continuous variables $x$ with SQP.
  5. Check convergence: If $f(x^{k+1}, y^{k+1}) - \alpha \le 10^{-3}$, declare optimality. Otherwise, append new supporting hyperplanes to the master MILP and repeat.

---

### 2.4 Milestone 4: Sovereign Python Bindings (W7)

#### 2.4.1 pybind11 Zero-Copy Architecture (`python/_core/`)
- **Zero Runtime Overhead:** Direct CPython C-API linkage with zero dynamic solver dependencies.
- **Buffer Protocol Integration:**
  NumPy 1D array (`py::array_t<double>`) wrapping of primal solution vectors without copying.
  Sparse constraint matrix ingestion directly from SciPy `scipy.sparse.csc_matrix` without intermediate conversion or memory reallocation.
- **Python API Surface:**
  ```python
  import markov_cero as mc

  # File solve
  res = mc.solve("model.mps", options=mc.SolveOptions(engine="milp", threads=4))

  # Model builder API
  m = mc.Model("blending")
  x = m.continuous_var("x", lb=0.0, ub=10.0)
  y = m.integer_var("y", lb=0, ub=5)
  m.minimize(3.0 * x + 2.0 * y)
  m.add_constraint(x + y <= 8.0, "c1")
  res = m.solve()

  # NLP Callback API
  nlp = mc.NlpModel(n_vars=2, n_ineq=1, n_eq=0)
  nlp.set_objective(lambda x: (x[0]-1)**2 + (x[1]-2)**2,
                    lambda x: np.array([2*(x[0]-1), 2*(x[1]-2)]))
  nlp.set_ineq(lambda x: np.array([x[0] + x[1] - 3.0]),
               lambda x: np.array([[1.0, 1.0]]))
  nlp.set_bounds(lb=[0.0, 0.0], ub=[5.0, 5.0])
  res = mc.solve_nlp(nlp)
  ```
- **Packaging & Build System:** `pyproject.toml` configured with `scikit-build-core` and `pybind11 >= 2.12`, supporting standard `pip install .` and `pip install -e . --no-build-isolation`.

---

### 2.5 Milestone 5: ML-Assisted Branching with Strict ML Best Practices (W2)

#### 2.5.1 Bipartite Graph Feature Representation
Following Zhang et al. (2025) and Kimiaei et al. (2025), represent each MILP branch-and-bound node as an undirected bipartite graph $\mathcal{G} = (\mathcal{V}_c, \mathcal{V}_v, \mathcal{E})$:
1. **Variable (Column) Nodes ($j \in \mathcal{V}_v$):**
   - Fractionality: $f_j = x_j^* - \lfloor x_j^* \rfloor$
   - Normalized objective coefficient: $c_j / \|c\|_2$
   - Up pseudocost: $\psi_j^+$, Down pseudocost: $\psi_j^-$
   - Pseudocost ratio: $\psi_j^+ / (\psi_j^- + \epsilon)$
   - Bound width: $\log(1.0 + u_j - l_j)$
   - Column nonzero density: $\text{nnz}(A_{*, j}) / m$
   - Variable type flag (binary vs general integer)
2. **Constraint (Row) Nodes ($i \in \mathcal{V}_c$):**
   - Normalized right-hand side: $b_i / \|b\|_2$
   - Row sense ($\le, =, \ge$)
   - Slack activity: $b_i - A_{i, *} x^*$
   - Dual value / activity from relaxation: $\pi_i$
   - Objective cosine alignment: $\frac{A_{i, *} \cdot c}{\|A_{i, *}\|_2 \|c\|_2}$
   - Row nonzero density: $\text{nnz}(A_{i, *}) / n$
3. **Edge Weights ($(i, j) \in \mathcal{E}$):**
   - Normalized coefficient: $A_{ij} / \|A_{i, *}\|_2$

#### 2.5.2 Strong Branching Data Pipeline & Strict ML Best Practices
- **Data Collection (`scripts/ml/collect_training_data.py`):**
  Run Markov-CERO with `--log-sb-features` on MIPLIB 2017 easy benchmark instances (~80 instances).
  At each node, compute exact strong branching scores $s_j^* = \Delta z_j^- \cdot \Delta z_j^+$.
  Serialize features and ground-truth scores to compressed NumPy arrays.
- **Strict ML Best Practices (Audit Requirement R20 / Prompt):**
  - **Data Partitioning:** Chronological / by-instance partitioning strictly before fitting normalizers:
    - Train split: 70% of instances
    - Validation split: 15% of instances
    - Test split: 15% of instances
    Zero data leakage across instance formulations.
  - **Outlier Handling & Scaling:** Robust scaling (median and interquartile range IQR) fitted strictly on the train split.
  - **Evaluation Metrics:**
    - Ranking metrics: Kendall's rank correlation coefficient $\tau$, Normalized Discounted Cumulative Gain at $k$ (NDCG@k for $k \in \{1, 3, 5\}$).
    - Regression metrics: Mean Squared Error (MSE), Mean Absolute Error (MAE).
    - Latency profiling: Forward pass inference time per node candidate set must not exceed 20% of an LP simplex pivot time.

#### 2.5.3 2-Layer Bipartite GCN Architecture & Zero-Dependency C++ Inference
- **Neural Network Architecture:**
  - Bipartite Graph Convolutional Network (~20,000 parameters):
    $$\mathbf{h}_v^{(l+1)} = \text{ReLU}\left( W_v^{(l)} \mathbf{h}_v^{(l)} + \sum_{c \in \mathcal{N}(v)} \alpha_{cv} W_c^{(l)} \mathbf{h}_c^{(l)} \right)$$
  - Tiny footprint: Exported to Int8 quantized ONNX (`data/ml_models/branching_scorer.onnx`, $< 25 \text{ KB}$).
- **Embedded C++ Inference (`src/milp/ml_branching/onnx_scorer.cpp`):**
  Uses embedded, header-only runtime (`ortinference.hpp`), completely avoiding dynamic linking to heavy external Python or PyTorch libraries.
- **Activation Gate:**
  Active ONLY when:
  1. `-DMARKOV_CERO_ENABLE_ML=ON` compiled.
  2. Model file `data/ml_models/branching_scorer.onnx` exists.
  3. Instance has $> 200$ fractional integer variables.
  4. User explicitly passes `--branching ml_gnn`.
  Fallback: Silently reverts to `pseudo_cost` branching if criteria are not met.
- **Verification Criterion:**
  Achieve equal or fewer branch-and-bound nodes than pseudo-cost branching on `stein15.mps`.

---

### 2.6 Milestone 6: Full Datasets & Comprehensive Solver Comparison (W8 + W9)

#### 2.6.1 Automated Dataset Download & Provenance Architecture
All external instances are downloaded on-demand and gitignored in `data/<suite>_full/`, accompanied by cryptographically verified SHA-256 provenance files:
- **Netlib LP Full Suite (97 instances):** Downloaded via `scripts/download_netlib_full.sh` from `http://www.netlib.org/lp/data/`. Provenance: `data/netlib_full/provenance.json`.
- **MIPLIB 2017 Benchmark Subset (~80 easy instances):** Downloaded via `scripts/download_miplib.py` from `https://miplib.zib.de/`. Provenance: `data/miplib_full/provenance.json`.
- **Mittelmann Benchmark Suites:** Downloaded via `scripts/download_mittelmann.py` from `http://plato.asu.edu/ftp/lptestset/`. Provenance: `data/mittelmann_full/provenance.json`.
- **Convex QPLIB Instances:** Downloaded via `scripts/download_qplib.py` from `https://qplib.io/` (convex subset). Provenance: `data/qplib_full/provenance.json`.
- **Literature Industrial Domain Cases:** Generated via `scripts/generators/gen_literature_cases.py`:
  - `data/cases/neiro_refinery_scheduling.mps` (Neiro & Pinto 2004)
  - `data/cases/li_crude_blending.mps` (Li et al. 2002)
  - `data/cases/capitanescu_dc_opf.mps` (Capitanescu 2011)
  - `data/cases/shapiro_network_flow.mps` (Shapiro 2001)
  - `data/cases/pochet_lot_sizing.mps` (Pochet & Wolsey 2006)
  Each accompanied by `<name>.provenance.json`.

#### 2.6.2 Automated Comparative Benchmarking Harness (`scripts/run_full_compare.py`)
- **Execution Architecture:**
  - Free Solvers (run locally as subprocesses):
    - `markov-cero`: `./build/markov-cero-solve`
    - `HiGHS`: Python interface (`highspy`)
    - `GLPK`: `glpsol --mps {file} --output /dev/null`
    - `COIN-OR CBC`: `cbc {file} solve stat`
    - `SCIP`: `scip -f {file} -q`
  - Commercial Solvers (lookup tables from Mittelmann):
    - CPLEX 22.1, Gurobi 11.0, FICO Xpress 41 scraped and stored in `data/mittelmann_tables/*.csv`.
- **Dolan-Moré Performance Profiles:**
  For solver $s$ and problem $p \in \mathcal{P}$:
  $$r_{p, s} = \frac{t_{p, s}}{\min_{s' \in \mathcal{S}} t_{p, s'}}$$
  Performance profile cumulative distribution:
  $$\rho_s(\tau) = \frac{1}{|\mathcal{P}|} \left| \left\{ p \in \mathcal{P} : r_{p, s} \le \tau \right\} \right|$$
  Generated and saved to:
  - `evidence/comparison/dolan_more_lp.svg`
  - `evidence/comparison/dolan_more_milp.svg`
  - `evidence/comparison/full_comparison_report.md`

---

## 3. Discovered Features Catalog

The following table catalogs all discovered features, specifications, inputs, outputs, error behaviors, and discovery sources across the 9 workstreams and Milestones 1–6:

## Features Discovered
| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|----------|---------|-------------|--------|---------|----------------|----------------|
| 1 | W5 (Numerics) | Sparse Normal Equations IPM | Replaces dense LU with sparse normal equations factorizer using `SparseLU` on $A D A^T$, scaling IPM to $m \ge 50,000$. | `SparseCanonicalModel`, diagonal $D=\text{diag}(x/s)$, RHS vector | Newton direction $(\Delta x, \Delta y, \Delta s)$, optimal vertex basis | If factorize fails, applies diagonal perturbation $\delta \in [10^{-10}, 10^{-2}]$; throws on total singularity | `ORIGINAL_REQUEST.md:27`, `implementation_plan.md:424` |
| 2 | W5 (Numerics) | PDLP Stagnation Detector | Monitors residual improvement; detects stalls when progress $<0.1\%$ over window of 1000 iterations. | Running primal/dual residuals from Chambolle-Pock iteration | `bool is_stagnant` | None (pure boolean detector) | `ORIGINAL_REQUEST.md:28`, `implementation_plan.md:454` |
| 3 | W5 (Numerics) | PDLP Dual Simplex Crossover | Extracts candidate basis from stagnated PDLP iterate using complementary slackness and warm-starts dual simplex. | Model CSC matrix, PDLP iterate $x_{\text{pdlp}}$ | Certified `BasisState`, exact vertex solution $\le 10^{-7}$ KKT | If extracted basis is singular, catches exception, returns PDLP solution with `convergence_note: "stagnated at tolerance floor"` | `ORIGINAL_REQUEST.md:28`, `implementation_plan.md:475` |
| 4 | W5 (Numerics) | ADMM Adaptive Penalty $\rho$ | Dynamically adjusts penalty parameter $\rho \in [10^{-6}, 10^6]$ based on primal vs dual residual ratio. | $\|r_{\text{prim}}\|_\infty, \|r_{\text{dual}}\|_\infty, \rho_k$ | Updated $\rho_{k+1}$, KKT refactorization trigger | Clamps $\rho$ within $[10^{-6}, 10^6]$; sets `numerical_error` if KKT refactorization fails | `ORIGINAL_REQUEST.md:29`, `implementation_plan.md:490` |
| 5 | W5 (Numerics) | Always-On Iterative Refinement | Executes single-pass iterative refinement with 80-bit `long double` residual accumulation on every LU solve. | Basis matrix $B$, RHS $b$, initial solution $x$ | Refined solution $x \leftarrow x + \Delta x$ | Early exit if $\|r\|_\infty < 10^{-14}$; checks finiteness of correction | `ORIGINAL_REQUEST.md:30`, `implementation_plan.md:514` |
| 6 | W5 (Numerics) | Structured NumericalDiagnostic | Emits detailed numerical diagnostic report on any non-optimal solver exit. | Solve failure state, residuals, matrix condition | Populated `NumericalDiagnostic` struct in `SolveResult` | Encapsulates failure details without crashing | `ORIGINAL_REQUEST.md:31`, `implementation_plan.md:544` |
| 7 | W6 (Model) | Sovereign Problem Classifier | Evaluates model structure, callbacks, and MPS sections to classify into LP, MILP, QP, MIQP, NLP, MINLP. | `Model`, `NlpModel`, MPS sections | `ProblemClass`, recommended solver engine | Defaults safely to continuous LP/QP if discrete flags absent | `ORIGINAL_REQUEST.md:34`, `implementation_plan.md:593` |
| 8 | W6 (Model) | Engine Auto-Selection Logic | Resolves optimal engine and backend using model density and problem class. | Problem class, NNZ, CLI flags | `resolved_engine` string (`pdlp`, `primal`, `milp`, `sqp`, etc.) | Warns and falls back if GPU requested on small instance | `ORIGINAL_REQUEST.md:34`, `implementation_plan.md:614` |
| 9 | W3 (GPU) | CUDA "all-major" Compilation | Compiles PTX and SASS for NVIDIA compute architectures sm_50 through sm_90. | CMake build configuration | Target binary with multi-architecture PTX/SASS | Warnings on unsupported sub-sm_50 cards | `ORIGINAL_REQUEST.md:35`, `implementation_plan.md:296` |
| 10 | W3 (GPU) | Runtime GPU Capability Guard | Verifies device compute capability $\ge 5.0$ before initializing CUDA context. | Device ID, CUDA device properties | Boolean availability, device metadata | Logs warning and falls back to CPU PDLP if major $< 5$ | `ORIGINAL_REQUEST.md:35`, `implementation_plan.md:307` |
| 11 | W3 (GPU) | GPU ADMM Step Kernel | Accelerates $P x + \rho x$ SpMV and box projection on CUDA for QP models with $\text{NNZ}(P) > 100k$. | Device CSR $P$, vectors $x, q, z, y$, scalars $\rho, \alpha$ | Updated device vector $x^{k+1}$ | Silent fallback to CPU ADMM on CUDA error or OOM | `ORIGINAL_REQUEST.md:36`, `implementation_plan.md:318` |
| 12 | W3 (GPU) | Scale Crossover Study Runner | Synthesizes scale instances up to 5M nonzeros and measures CPU vs GPU crossover point. | Scale parameter ($10^5$ to $5 \times 10^6$), solver binary | CSV records in `evidence/benchmarks/crossover_study.csv` | Flags instances where GPU fails to achieve speedup | `ORIGINAL_REQUEST.md:37`, `implementation_plan.md:340` |
| 13 | W4 (GPU) | Hardware Backend Abstraction | Declares abstraction boundary for CPU, NVIDIA CUDA, and future AMD HIP / Intel OneAPI. | Backend enum | Execution dispatch pointer | Reports unsupported backend and routes to CPU | `implementation_plan.md:378` |
| 14 | W1 (NLP) | NlpModel Programmatic Callback API | Enables user-supplied C++ and Python function objects for $f, \nabla f, g, J_g, h, J_h$. | Callbacks, dimensions, variable bounds | Evaluated values and Jacobians | Throws `std::invalid_argument` on dimension mismatch | `ORIGINAL_REQUEST.md:42`, `implementation_plan.md:62` |
| 15 | W1 (NLP) | MPS `NLOBJ` / `NLCON` Section Parser | Parses polynomial objective and constraint terms in extended MPS files. | MPS file stream | Quadratic/polynomial model data | Syntax error emitted with line number | `ORIGINAL_REQUEST.md:42`, `implementation_plan.md:67` |
| 16 | W1 (NLP) | L-BFGS-B Hessian Approximator | Limited-memory BFGS with bound constraints, memory $m=10$, two-loop recursion. | Gradients $\nabla f$, step vectors $s_k, y_k$ | Hessian-vector product $B_k d$ | Resets to identity if $s_k^T y_k \le 0$ (max 3 resets) | `ORIGINAL_REQUEST.md:40`, `implementation_plan.md:96` |
| 17 | W1 (NLP) | Armijo-Wolfe Line Search | $\ell_1$ merit function line search enforcing Armijo ($c_1=10^{-4}$) and Wolfe ($c_2=0.9$) criteria. | Merit parameter $\mu$, search direction $d$ | Step length $\alpha \in (0, 1]$ | Shrinks step; aborts if step $< 10^{-12}$ | `ORIGINAL_REQUEST.md:40`, `implementation_plan.md:102` |
| 18 | W1 (NLP) | Sequential Quadratic Programming (SQP) Solver | Outer SQP loop solving QP subproblems to find local minimum of general NLP. | `NlpModel`, options | Primal solution $x^*$, multipliers, KKT certificate | Emits `NumericalDiagnostic` on failure to converge | `ORIGINAL_REQUEST.md:40`, `implementation_plan.md:93` |
| 19 | W1 (NLP) | KKT Residual Verifier for NLP | Certifies first-order optimality conditions $\le 10^{-6}$ for general NLP. | $x, \lambda, \nu$, gradient, Jacobians | Primal/dual infeasibility, complementarity | Flags non-optimal if residual exceeds tolerance | `ORIGINAL_REQUEST.md:40`, `implementation_plan.md:105` |
| 20 | W1 (MINLP) | Convex Outer Approximation Solver | Solves convex MINLP via alternating master MILP and continuous NLP subproblems. | Convex MINLP model, tolerances | Integer-feasible optimal solution | Declares `non_convex_minlp` if root relaxation Hessian not PSD | `ORIGINAL_REQUEST.md:41`, `implementation_plan.md:116` |
| 21 | W1 (MINLP) | Linearization Cut Generator | Generates supporting hyperplanes $g_i(x^k) + \nabla g_i(x^k)^T(x - x^k) \le 0$ for master MILP. | NLP solution $x^k$, constraint gradients | Linear constraint triplets | Skips redundant or parallel cuts | `ORIGINAL_REQUEST.md:41`, `implementation_plan.md:128` |
| 22 | W7 (Python) | pybind11 CPython Core Extension | Exposes sovereign C++ solver to Python as `markov_cero._core`. | CPython runtime | Python module object | Translates C++ exceptions to Python `RuntimeError` | `ORIGINAL_REQUEST.md:45`, `implementation_plan.md:649` |
| 23 | W7 (Python) | Zero-Copy NumPy Buffer Protocol | Direct zero-copy access between C++ vectors/CSC arrays and NumPy/SciPy. | NumPy ndarray, SciPy CSC | C++ memory views | Verifies contiguous C-order layout and data types | `ORIGINAL_REQUEST.md:46`, `implementation_plan.md:649` |
| 24 | W7 (Python) | High-Level Python Model Builder | Object-oriented model construction API (`mc.Model`, continuous/integer variables). | Variable bounds, constraints, objective | Markov-CERO C++ `Model` | Raises `ValueError` on inconsistent bounds | `ORIGINAL_REQUEST.md:45`, `implementation_plan.md:670` |
| 25 | W2 (ML) | Bipartite Graph Feature Extractor | Extracts row, column, and edge features for B&B candidate variable scoring. | LP tableau, model matrix, pseudocosts | `NodeFeatureVector` | Returns empty features if model has no fractional variables | `ORIGINAL_REQUEST.md:50`, `implementation_plan.md:200` |
| 26 | W2 (ML) | Strong Branching Data Logger | Logs bipartite graphs and exact strong branching scores on MIPLIB easy instances. | Solver B&B search nodes | Compressed `.npy` / `.npz` feature-score datasets | Skips logging if disk space or write error occurs | `ORIGINAL_REQUEST.md:50`, `implementation_plan.md:211` |
| 27 | W2 (ML) | Bipartite GCN Model Training Script | Offline PyTorch script training 2-layer GCN with ranking loss (Kendall $\tau$, NDCG). | Feature-score dataset splits | Trained PyTorch weights | Validates training loss convergence | `ORIGINAL_REQUEST.md:51`, `implementation_plan.md:219` |
| 28 | W2 (ML) | Int8 ONNX Quantization Exporter | Quantizes PyTorch GCN weights to Int8 ONNX format ($\approx 20 \text{ KB}$). | PyTorch checkpoint | `branching_scorer.onnx` | Verifies ONNX validity via `onnx.checker` | `ORIGINAL_REQUEST.md:52`, `implementation_plan.md:229` |
| 29 | W2 (ML) | C++ Embedded ONNX Scorer | Lightweight C++ inference engine scoring fractional variables without external libraries. | `NodeFeatureVector`, fractional indices | Candidate score vector | Falls back to pseudo-cost if scorer fails | `ORIGINAL_REQUEST.md:52`, `implementation_plan.md:236` |
| 30 | W8 (Bench) | Netlib Full Suite Downloader | Shell script fetching all 97 Netlib LP instances with SHA-256 validation. | Netlib HTTP URL | Decompressed `.mps` files, `provenance.json` | Aborts on checksum mismatch | `ORIGINAL_REQUEST.md:55`, `implementation_plan.md:789` |
| 31 | W8 (Bench) | MIPLIB 2017 Downloader | Python script downloading and decompressing MIPLIB 2017 benchmark easy subset. | MIPLIB ZIB URL | Decompressed `.mps` files, `provenance.json` | Validates archive integrity | `ORIGINAL_REQUEST.md:55`, `implementation_plan.md:794` |
| 32 | W8 (Bench) | Mittelmann Testsets Downloader | Fetches LP, MILP, and QP benchmark models from plato.asu.edu. | ASU FTP/HTTP URL | Benchmark `.mps` files, `provenance.json` | Flags missing or unreachable instances | `ORIGINAL_REQUEST.md:55`, `implementation_plan.md:800` |
| 33 | W8 (Bench) | QPLIB Convex Downloader | Downloads convex QP instances from qplib.io and converts to MPS via `import_qplib.py`. | QPLIB repository | Converted `.mps` / `.qps` files, `provenance.json` | Rejects non-convex instances | `ORIGINAL_REQUEST.md:55`, `implementation_plan.md:808` |
| 34 | W8 (Bench) | Literature Industrial Case Generator | Generates 5 literature industrial case models (refinery, blending, OPF, supply chain). | Mathematical parameters from papers | Verified MPS files with provenance JSONs | Validates model bounds and feasibility | `ORIGINAL_REQUEST.md:55`, `implementation_plan.md:835` |
| 35 | W9 (Comp) | Automated Full Benchmark Runner | Runs solver across full benchmark suites, extracting objective, time, and residuals. | Benchmark suite name, timeout, thread count | Standardized CSV results in `evidence/` | Records timeout or failure without terminating suite | `ORIGINAL_REQUEST.md:56`, `implementation_plan.md:819` |
| 36 | W9 (Comp) | Mittelmann Web Scraper | Scrapes published commercial solver benchmark results (CPLEX, Gurobi, Xpress). | plato.asu.edu HTML tables | Reference CSV files in `data/mittelmann_tables/` | Warns if table format on ASU changes | `ORIGINAL_REQUEST.md:56`, `implementation_plan.md:912` |
| 37 | W9 (Comp) | Comparative Multi-Solver Harness | Executes Markov-CERO against local free solvers (HiGHS, GLPK, CBC, SCIP). | Model files, solver configurations | Comparison CSVs and markdown reports | Handles missing solver binaries gracefully | `ORIGINAL_REQUEST.md:56`, `implementation_plan.md:887` |
| 38 | W9 (Comp) | Dolan-Moré Profile Generator | Computes performance ratios and plots publication-grade SVG performance curves. | Comparison CSV results | `dolan_more_lp.svg`, `dolan_more_milp.svg` | Includes failed/timed-out instances as penalty | `ORIGINAL_REQUEST.md:57`, `implementation_plan.md:941` |

---

## 4. Edge Cases & Numerical Boundaries

## Edge Cases
| # | Feature | Input | Observed Behavior |
|---|---------|-------|-------------------|
| 1 | SparseLU IPM | Singular normal matrix ($A D A^T$ rank deficient, e.g. redundant constraints) | Traps zero pivot ($< 10^{-14}$); triggers diagonal perturbation ladder $\delta = 10^{-10} \times 100^k$ up to $10^{-2}$. If still singular, aborts with `NumericalDiagnostic` stating singular iteration. |
| 2 | SparseLU IPM | Large sparse matrix ($m > 200$, Netlib `sc205`, `share1b`) | Solves without memory explosion; sparse Cholesky/LU memory scales linearly with factor nonzeros ($O(\text{nnz}(L+U))$) rather than $O(m^2)$. |
| 3 | PDLP Stagnation | Slow oscillatory convergence on Netlib `kb2`, `lotfi`, `beaconfd` | Stagnation detector triggers after 1000 iterations with $< 0.1\%$ progress; initiates basis extraction and crossover into dual simplex. |
| 4 | PDLP Crossover | Extracted candidate basis is rank-deficient | Dual simplex basis factorization detects singular basis; catches exception and falls back to PDLP solution with `convergence_note: "stagnated at tolerance floor"`. |
| 5 | ADMM QP Solver | Ill-conditioned QP with extreme primal-to-dual residual imbalance | Primal residual $> 10 \times$ dual residual triggers $\rho \leftarrow \rho \times 2.0$; re-factors KKT matrix. Increments `admm_rho_updates` counter. |
| 6 | ADMM QP Solver | Non-convex quadratic objective matrix $P$ (negative eigenvalues) | LDLT factorization detects negative inertia or pivot non-existence; immediately halts with `QpStatus::non_convex`. |
| 7 | SparseLU Refinement | Ill-conditioned basis ($10^5 \le \kappa \le 10^8$) previously bypassing refinement | Always-on refinement computes residual in `long double`; detects $\|r\|_\infty \ge 10^{-14}$; executes single iterative correction step, reducing forward error to $< 10^{-6}$. |
| 8 | Problem Classifier | Model with quadratic objective and integer variables | Accurately assigns class `ProblemClass::MIQP` and selects `miqp` branch-and-cut engine. |
| 9 | Problem Classifier | LP instance with 65,000 nonzeros and no discrete variables | Auto-selects `pdlp` engine because $\text{NNZ} > 50,000$, overriding simplex default. |
| 10 | GPU Device Guard | Legacy NVIDIA card with compute capability sm_35 (Kepler) | Runtime check in `device.cpp` detects `major < 5`; emits warning and returns false, falling back safely to CPU PDLP. |
| 11 | GPU ADMM Kernel | QP model with $\text{NNZ}(P) < 100,000$ and `--backend gpu` | Checks $\text{NNZ}(P)$ threshold; executes CPU ADMM to avoid PCIe transfer overhead penalties. |
| 12 | SQP Solver | Rosenbrock non-convex test problem from $(-1.2, 1.0)$ | Successfully converges to local minimum $(1.0, 1.0)$ within objective tolerance $10^{-3}$; KKT residual $\le 10^{-6}$. |
| 13 | SQP Solver | Indefinite L-BFGS-B Hessian approximation during non-convex step | ADMM QP subproblem solver flags non-convexity; SQP catches failure, resets Hessian to scaled identity, and restarts from current iterate (up to 3 resets). |
| 14 | MINLP Solver | Integer variables present in nonlinear polynomial constraints | Outer approximation linearizes nonlinear constraints at NLP subproblem solution; master MILP successfully enforces integer lattice feasibility. |
| 15 | Python Bindings | Non-contiguous or Fortran-ordered NumPy array passed to solver | Buffer protocol layer validates contiguous C-order; raises descriptive Python `ValueError` instead of segfaulting. |
| 16 | ML Branching | Instance with $< 200$ fractional integer variables | Bypasses GNN forward pass; automatically uses fast classical `pseudo_cost` branching. |
| 17 | ML Branching | Evaluation on `stein15.mps` with `--branching ml_gnn` | GNN scoring achieves equal or fewer total branch-and-bound nodes than pseudo-cost branching. |
| 18 | Solver Comparison | Benchmark model times out at 300 seconds | Harness kills process, records status as `TimeLimit`, and assigns penalty ratio in Dolan-Moré profile without crashing the suite. |

---

## 5. File Action & Layout Compliance Matrix

All code and test modifications must adhere strictly to repository layout conventions. No source code or tests may be placed in `.agents/`.

### 5.1 New C++ Source Files (to be added to `CMakeLists.txt`)
- `src/nlp/nlp_model.cpp`
- `src/nlp/sqp_solver.cpp`
- `src/nlp/lbfgs.cpp`
- `src/nlp/merit_function.cpp`
- `src/nlp/nlp_verifier.cpp`
- `src/minlp/minlp_solver.cpp`
- `src/minlp/outer_approx.cpp`
- `src/io/nlobj_parser.cpp`
- `src/model/classifier.cpp`
- `src/milp/ml_branching/feature_extractor.cpp`
- `src/milp/ml_branching/onnx_scorer.cpp` (conditional: `MARKOV_CERO_ENABLE_ML`)
- `src/milp/ml_branching/training_logger.cpp` (conditional: `MARKOV_CERO_ENABLE_ML`)
- `gpu/src/admm_matvec.cpp`
- `gpu/kernels/admm_step.cu` (conditional: `MARKOV_CERO_ENABLE_CUDA`)

### 5.2 New C++ Header Files
- `include/markov_cero/nlp/nlp_model.hpp`
- `include/markov_cero/nlp/sqp_solver.hpp`
- `include/markov_cero/nlp/lbfgs.hpp`
- `include/markov_cero/nlp/nlp_verifier.hpp`
- `include/markov_cero/minlp/minlp_solver.hpp`
- `include/markov_cero/minlp/outer_approx.hpp`
- `include/markov_cero/model/classifier.hpp`
- `include/markov_cero/milp/ml_branching/feature_extractor.hpp`
- `include/markov_cero/milp/ml_branching/onnx_scorer.hpp`
- `gpu/include/markov_cero/gpu/admm_step.hpp`
- `gpu/include/markov_cero/gpu/backend.hpp`

### 5.3 New Test Files
- `tests/nlp_sqp_test.cpp`
- `tests/nlp_rosenbrock_test.cpp`
- `tests/nlp_constrained_test.cpp`
- `tests/minlp_basic_test.cpp`
- `tests/nlobj_parser_test.cpp`
- `tests/ipm_large_test.cpp`
- `tests/pdlp_crossover_test.cpp`
- `tests/qp_adaptive_rho_test.cpp`
- `tests/numerical_diagnostic_test.cpp`
- `tests/classifier_test.cpp`
- `tests/ml_branching_test.cpp`
- `tests/gpu_fallback_test.cpp`
- `gpu/tests/admm_gpu_test.cpp`

### 5.4 Modified Existing Files
- `src/linalg/sparse_basis.cpp`: Always-on single pass refinement with `long double` accumulators (D-17).
- `src/lp/interior/ipm.cpp`: Sparse normal equations factorizer using `SparseLU` (D-14).
- `src/lp/first_order/pdlp.cpp`: Stagnation detector + dual simplex crossover (D-15).
- `src/qp/admm_solver.cpp`: Boyd et al. (2011) adaptive $\rho$ update rule (D-16).
- `src/api/api.cpp`: Classifier integration, NLP/MINLP dispatch, and universal `NumericalDiagnostic` population.
- `src/milp/branch_selector.cpp`: Wire `OnnxBranchingScorer` when `--branching ml_gnn` active.
- `src/io/mps.cpp`: Parse `NLOBJ` / `NLCON` sections.
- `gpu/src/device.cpp`: Runtime sm_50 compute capability check.
- `apps/markov_cero_solve.cpp`: Add CLI flags (`--engine nlp|minlp`, `--branching ml_gnn`, etc.).
- `include/markov_cero/api/solve.hpp`: Add `NumericalDiagnostic` and `ProblemClass` fields to `SolveResult`.
- `CMakeLists.txt`: Add targets, update `CMAKE_CUDA_ARCHITECTURES="all-major"`, add ML and Python options.
- `STATUS.md`: Sync R1–R20 matrix and milestone completion.

### 5.5 New Python Scripts & Tools
- `scripts/download_netlib_full.sh`
- `scripts/download_miplib.py`
- `scripts/download_mittelmann.py`
- `scripts/download_qplib.py`
- `scripts/generate_large_scale_study.py`
- `scripts/run_full_benchmark.py`
- `scripts/run_full_compare.py`
- `scripts/scrape_mittelmann.py`
- `scripts/generators/gen_literature_cases.py`
- `scripts/ml/collect_training_data.py`
- `scripts/ml/train_branching_gnn.py`
- `scripts/ml/export_onnx.py`

### 5.6 New Documentation
- `docs/nlobj_format.md`: Specification for custom polynomial MPS extension.
- `docs/engine_selection.md`: Specification for model classification decision tree and engine dispatch table.

---

## 6. Verification Gates & Acceptance Criteria

| Milestone | Gate Verification Command | Pass Criteria |
|---|---|---|
| **M1: Numerics** | `ctest --output-on-failure` | 100% pass on all existing 44 CTests; `ipm_large_test` solves Netlib $m \ge 200$ (`sc205`, `share1b`); `pdlp_crossover_test` certifies KKT $\le 10^{-7}$ on `kb2`, `lotfi`, `beaconfd`; all non-optimal runs emit structured `NumericalDiagnostic`. |
| **M2: Classifier & GPU** | `ctest -R "classifier|gpu"` | `classifier_test` correctly identifies all 6 problem classes; GPU builds with `all-major` sm_50–sm_90; `crossover_study.csv` generated up to 5M nonzeros. |
| **M3: NLP & MINLP** | `ctest -R "nlp|minlp"` | SQP reaches certified KKT $\le 10^{-6}$ on convex NLP; Rosenbrock reaches $(1, 1)$ within $10^{-3}$; MINLP outer approximation solves convex mixed-integer nonlinear problems. |
| **M4: Python** | `pytest python/tests/ -v` | Python extension builds cleanly via `pip install .`; zero-copy NumPy buffers operate correctly; all pytest suites green. |
| **M5: ML Branching** | `ctest -R ml_branching` | Int8 ONNX model $< 25 \text{ KB}$; zero-dependency C++ inference runs; B&B node count on `stein15.mps` with `--branching ml_gnn` $\le$ node count with `pseudo_cost`. |
| **M6: Benchmarks** | `python3 scripts/run_full_compare.py` | Provenance JSONs committed for Netlib (97), MIPLIB (easy), Mittelmann, QPLIB; local benchmarks vs HiGHS, GLPK, CBC, SCIP complete; Dolan-Moré SVG performance curves generated. |
