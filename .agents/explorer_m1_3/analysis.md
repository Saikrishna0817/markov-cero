# Technical Analysis: ADMM Adaptive Rho, Always-On Refinement, and NumericalDiagnostic

**Agent**: `explorer_m1_3`  
**Milestone**: Milestone 1 — Numerical Accuracy Hardening (W5)  
**Workspace**: `/home/saikrishna/markov-initial-build`  
**Date**: 2026-09-26T19:35:00Z  

---

## 1. Executive Summary

This report establishes the complete architectural and mathematical design for three critical components of Milestone 1 (Numerical Accuracy Hardening) in `markov-cero`:
1. **ADMM Adaptive Penalty Parameter $\rho$ Update & KKT Refactorization Counter** (`src/qp/admm_solver.cpp`, `include/markov_cero/qp/admm_solver.hpp`):
   Transitioning from heuristic OSQP residual scaling to the canonical Boyd et al. (2011) adaptation scheme with parameter bounds $\rho \in [10^{-6}, 10^6]$, residual imbalance factor $\mu = 10$, scaling rate $\tau = 2$, SQD KKT numeric LDLᵀ refactorization, and accurate tracking of `refactorization_count` in `QpSolution`.
2. **Always-On Iterative Refinement** (`src/linalg/sparse_basis.cpp`, `include/markov_cero/linalg/sparse_basis.hpp`):
   Transitioning from selective proxy-gated refinement to unconditional extended-precision (`long double`) residual evaluation on every solve and transpose solve, with early exit at $\|r\|_\infty < 10^{-14}$, eliminating silent backward error drift while maintaining zero overhead on exact solutions.
3. **Structured `NumericalDiagnostic` Across All Engines** (`include/markov_cero/api/solve.hpp`, `src/api/api.cpp`, `apps/json_output.hpp`):
   Introducing `struct NumericalDiagnostic` (capturing primal/dual residuals, condition estimate, exact failure site, and actionable recovery recommendations), embedding it in `SolveResult`, populating it across all engines (QP, PDLP, simplex, IPM, MILP, parser, and verification layers), and serializing it into CLI JSON outputs to eliminate silent failures.

Baseline testing confirms **59/59 CTest targets passing (100%)**. The proposed strategies preserve complete backwards compatibility with existing test fixtures while meeting all Milestone 1 acceptance criteria.

---

## 2. Component 1: ADMM Adaptive Penalty Parameter $\rho$ & Refactorization Counter

### 2.1 Theoretical Framework (Boyd et al. 2011)
In alternating direction method of multipliers (ADMM) for convex quadratic programming:
$$\min_{x} \frac{1}{2} x^T P x + q^T x \quad \text{s.t.} \quad l \le A x \le u$$
Splitting with slack variable $z \in [l, u]$ yields the augmented Lagrangian:
$$L_\rho(x, z, y) = \frac{1}{2} x^T P x + q^T x + I_{[l, u]}(z) + y^T (A x - z) + \frac{\rho}{2} \|A x - z\|_2^2 + \frac{\sigma}{2} \|x - x^k\|_2^2$$
The primal and dual residuals at iteration $k$ are defined as:
- **Primal residual**: $r_{\text{prim}}^k = A x^k - z^k \in \mathbb{R}^m$
- **Dual residual**: $r_{\text{dual}}^k = P x^k + q + A^T y^k \in \mathbb{R}^n$

Following Boyd et al. (2011) §3.4.1, optimal convergence requires balancing the primal and dual residual norms:
- If $\|r_{\text{prim}}\|_\infty > \mu \|r_{\text{dual}}\|_\infty$, the penalty parameter $\rho$ is too small (primal feasibility lags dual feasibility). Increasing $\rho$ penalizes constraint violations more heavily.
- If $\|r_{\text{dual}}\|_\infty > \mu \|r_{\text{prim}}\|_\infty$, $\rho$ is too large (dual feasibility lags primal feasibility). Decreasing $\rho$ eases the augmented Lagrangian penalty, allowing $x$ to adjust toward optimality.

The authoritative parameters are:
- Residual imbalance threshold: $\mu = 10.0$
- Multiplicative increase factor: $\tau_{\text{incr}} = 2.0$
- Multiplicative decrease factor: $\tau_{\text{decr}} = 2.0$
- Dynamic range limits: $\rho \in [10^{-6}, 10^6]$

### 2.2 Current Codebase State
In `include/markov_cero/qp/admm_solver.hpp` (lines 40–50):
```cpp
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
```
Notice `refactorization_count` is absent.

In `src/qp/admm_solver.cpp` (lines 304–322):
```cpp
// 9. Adaptive penalty parameter update (OSQP Section 5.2)
if (options_.adaptive_rho && iter > 0 &&
    (iter % options_.adaptive_rho_interval == 0)) {
    const double s_prim =
        norm_prim / (std::max({inf_norm(Ax), inf_norm(z), 1.0}));
    const double s_dual =
        norm_dual / (std::max({inf_norm(Px), inf_norm(ATy),
                               inf_norm(model.q), 1.0}));
    if (s_dual > 1e-12 && s_prim > 1e-12) {
        const double ratio = s_prim / s_dual;
        if (ratio > 5.0 || ratio < 0.2) {
            double scale = std::clamp(std::sqrt(ratio), 0.33, 3.0);
            for (std::size_t i = 0; i < m; ++i) {
                rho[i] = std::clamp(rho[i] * scale, 1e-3, 1e4);
            }
            kkt.update_numeric(model.P, model.A, options_.sigma, rho);
        }
    }
}
```
Deficiencies identified:
1. Uses normalized residuals `s_prim` and `s_dual` rather than direct infinity norms $\|r_{\text{prim}}\|_\infty$ and $\|r_{\text{dual}}\|_\infty$.
2. Uses continuous scaling `scale = std::clamp(std::sqrt(ratio), 0.33, 3.0)` rather than Boyd's discrete $\tau = 2$ doubling/halving.
3. Clamps $\rho$ to $[10^{-3}, 10^4]$ instead of the required $[10^{-6}, 10^6]$.
4. Ignores the boolean return of `kkt.update_numeric`.
5. Does not track or increment `refactorization_count`.

### 2.3 Proposed Implementation Strategy
#### 1. Header Modification (`include/markov_cero/qp/admm_solver.hpp`)
Add `std::size_t refactorization_count{0};` to `QpSolution`:
```cpp
struct QpSolution {
    QpStatus status{QpStatus::numerical_error};
    double objective_value{0.0};
    std::vector<double> x;
    std::vector<double> z;
    std::vector<double> y;
    std::size_t iterations{0};
    std::size_t refactorization_count{0};
    double solve_time_seconds{0.0};
    double primal_residual{0.0};
    double dual_residual{0.0};
};
```

#### 2. Solver Implementation (`src/qp/admm_solver.cpp`)
Track scalar penalty parameter `current_rho = options_.rho_init;` and replace lines 304–322:
```cpp
// 9. Adaptive penalty parameter update (Boyd et al. 2011 §3.4.1)
if (options_.adaptive_rho && iter > 0 &&
    (iter % options_.adaptive_rho_interval == 0)) {
    double new_rho = current_rho;
    if (norm_prim > 10.0 * norm_dual) {
        new_rho = std::min(2.0 * current_rho, 1e6);
    } else if (norm_dual > 10.0 * norm_prim) {
        new_rho = std::max(current_rho / 2.0, 1e-6);
    }

    if (new_rho != current_rho) {
        current_rho = new_rho;
        std::fill(rho.begin(), rho.end(), current_rho);
        if (!kkt.update_numeric(model.P, model.A, options_.sigma, rho)) {
            sol.status = QpStatus::numerical_error;
            break;
        }
        ++sol.refactorization_count;
    }
}
```

#### 3. Dual Variable Invariance Under $\rho$ Updates
In `admm_solver.cpp`, dual variable $y$ represents unscaled Lagrange multipliers ($r_{\text{dual}} = P x + q + A^T y$). Unlike scaled-form ADMM (where $u = y / \rho$ requires rescaling $u \leftarrow u \cdot \rho_{\text{old}} / \rho_{\text{new}}$), the unscaled formulation maintains physical multiplier values across $\rho$ updates without modification.

#### 4. KKT LDLᵀ Sparsity Pattern Invariance
The augmented KKT system is:
$$K = \begin{bmatrix} P + \sigma I & A^T \\ A & -\text{diag}(\rho)^{-1} \end{bmatrix}$$
Because $\rho \in [10^{-6}, 10^6]$, every diagonal entry $-\rho_i^{-1} \in [-10^6, -10^{-6}]$ is strictly non-zero. The non-zero pattern of $K$ never changes when $\rho$ updates. Therefore, `kkt.update_numeric()` performs numeric LDLᵀ factorization (`ldl_numeric`) in $O(\text{nnz}(L))$ time using the cached elimination tree and symbolic structure, avoiding redundant $O(\text{nnz}(A))$ symbolic analysis.

---

## 3. Component 2: Always-On Iterative Refinement in `src/linalg/sparse_basis.cpp`

### 3.1 Theoretical Framework (Skeel 1980)
In simplex and basis inversion operations, solving $B x = b$ or $B^T y = c_B$ suffers from finite-precision accumulation.
Given computed solution $\hat{x}$, Skeel (1980) iterative refinement proceeds as:
1. **Extended-Precision Residual**: Compute residual vector in IEEE 754 extended precision (`long double`, 80-bit on x86_64):
   $$r_i = \text{fl}_{80}\left(b_i - \sum_{j} B_{ij} \hat{x}_j\right)$$
   Crucially, accumulating in `long double` ensures the residual calculation has higher precision than the base solve; otherwise, refinement operates on truncation noise.
2. **Early Exit Guard**:
   $$\|r\|_\infty = \max_i |r_i|$$
   If $\|r\|_\infty < 10^{-14}$, the residual is already within machine epsilon precision. The solve terminates immediately without performing a correction step.
3. **Correction Solve**:
   If $\|r\|_\infty \ge 10^{-14}$, solve for correction vector:
   $$B \Delta x = r$$
   using the existing LU factors and eta update chain.
4. **Update**:
   $$\hat{x} \leftarrow \hat{x} + \Delta x$$
   Repeat up to `maximum_refinement_steps` (default 2).

### 3.2 Current Codebase State
In `src/linalg/sparse_basis.cpp` (lines 552–565, 605–606, 617–618):
- `refinement_required()` selectively triggers refinement only when:
  - `updates_.size() >= options_.refinement_trigger_updates` (default 16) OR
  - `growth_factor > options_.refinement_trigger_growth` (default 100) OR
  - `sparse_condition_estimate > options_.refinement_trigger_condition` (default $10^8$).
- In `solve` and `solve_transpose`, refinement is guarded by `if (refinement_required())`.
- In `refine()` (line 574), early exit checks only `if (worst == 0) break;`.

### 3.3 Proposed Implementation Strategy
#### 1. Always-On Invocations in `solve()` and `solve_transpose()`
Replace selective checks with unconditional execution whenever `options_.maximum_refinement_steps > 0`:
```cpp
std::vector<double> SparseBasisFactorization::solve(const std::vector<double>& rhs) {
    statistics_.last_rhs_nonzeros = count_nonzero(rhs);
    auto x = base_.solve(rhs);
    apply_updates(x);
    for (double v : x)
        require_finite(v, "non-finite eta solve result");
    if (options_.maximum_refinement_steps > 0)
        x = refine(std::move(x), rhs, false);
    statistics_.last_solution_nonzeros = count_nonzero(x);
    return x;
}

std::vector<double> SparseBasisFactorization::solve_transpose(const std::vector<double>& rhs) {
    statistics_.last_rhs_nonzeros = count_nonzero(rhs);
    if (rhs.size() != current_basis_.rows)
        throw std::invalid_argument("eta transpose dimension mismatch");
    std::vector<double> work = rhs;
    apply_updates_transpose(work);
    auto x = base_.solve_transpose(work);
    if (options_.maximum_refinement_steps > 0)
        x = refine(std::move(x), rhs, true);
    statistics_.last_solution_nonzeros = count_nonzero(x);
    return x;
}
```

#### 2. Early Exit at $\|r\|_\infty < 10^{-14}$ in `refine()`
Update lines 566–598 in `src/linalg/sparse_basis.cpp`:
```cpp
std::vector<double> SparseBasisFactorization::refine(std::vector<double> x,
                                                     const std::vector<double>& rhs,
                                                     bool transpose) {
    for (std::size_t step = 0; step < options_.maximum_refinement_steps; ++step) {
        const std::vector<double> r = residual_vector(rhs, x, transpose);
        double worst = 0.0;
        for (double v : r)
            worst = std::max(worst, std::abs(v));
        if (worst < 1e-14)
            break;  // early exit: residual already at or below machine noise level

        std::vector<double> correction;
        if (transpose) {
            correction = base_.solve_transpose(r);
            apply_updates_transpose(correction);
        } else {
            correction = base_.solve(r);
            apply_updates(correction);
        }

        bool changed = false;
        for (std::size_t i = 0; i < x.size(); ++i) {
            const double updated = x[i] + correction[i];
            if (updated != x[i] && std::isfinite(updated)) {
                x[i] = updated;
                changed = true;
            }
        }
        ++statistics_.refinement_attempts;
        if (!changed)
            break;
        ++statistics_.refinements_applied;
    }
    return x;
}
```

#### 3. Preservation of Test Invariants in `tests/sparse_basis_test.cpp`
Crucially, placing `if (worst < 1e-14) break;` **before** `++statistics_.refinement_attempts;` guarantees that:
- For clean integer/well-conditioned bases (such as `small` in `tests/sparse_basis_test.cpp:209`), the initial residual is $< 10^{-14}$, so the loop breaks immediately at step 0 without solving for a correction or incrementing `refinement_attempts`. Thus, `small.statistics().refinement_attempts == 0` remains strictly true!
- For `moler` (lines 160–173), unit pivots on integer entries compute exact results with residual $0.0 < 10^{-14}$, ensuring `moler.statistics().refinement_attempts == 0` remains strictly true.
- For `ill_on` (lines 199–205), ill-conditioning ($\kappa \approx 10^9$) causes residual $\ge 10^{-14}$, triggering the correction solve and ensuring `refinement_attempts >= 1` and `refinements_applied >= 1`.
- For `refine_off` (`maximum_refinement_steps = 0`), the loop does not run, ensuring `refinements_applied == 0`.

---

## 4. Component 3: Structured `NumericalDiagnostic` Across All Engines

### 4.1 Interface Specification
In `include/markov_cero/api/solve.hpp`:
```cpp
namespace markov_cero::api {

struct NumericalDiagnostic {
    double primal_residual{0.0};
    double dual_residual{0.0};
    double condition_estimate{0.0};
    std::string failure_site;
    std::string suggested_recovery;
};

struct SolveResult {
    lp::reference::SolveStatus status{lp::reference::SolveStatus::numerical_failure};
    std::string message;
    std::string resolved_engine = "auto";
    NumericalDiagnostic diagnostic;   // <--- Structured numerical diagnostic

    std::size_t model_rows = 0;
    std::size_t model_cols = 0;
    std::size_t model_nnz = 0;

    std::vector<double> primal;
    double objective = 0.0;
    std::vector<double> original_primal;
    double original_objective = 0.0;

    bool verified = false;
    bool canonical_verified = false;
    bool original_verified = false;
    std::string original_message;
    verify::PrimalVerificationReport primal_report;
    verify::ReferenceVerification canonical_report;

    bool used_warm_start = false;
    bool used_cold_fallback = false;

    double runtime_ms = 0.0;
    std::size_t nodes_explored = 0;
    std::size_t lp_iterations = 0;
    double best_bound = 0.0;
    double relative_gap = 0.0;
    std::size_t cuts_generated = 0;
    std::size_t heuristics_found = 0;
    std::size_t phase_one_iterations = 0;
    std::size_t phase_two_iterations = 0;

    double pdlp_primal_infeasibility = 0.0;
    double pdlp_dual_infeasibility = 0.0;
    double pdlp_duality_gap = 0.0;
    double pdlp_h2d_ms = 0.0;
    double pdlp_kernel_ms = 0.0;
    double pdlp_d2h_ms = 0.0;
    double pdlp_total_ms = 0.0;

    bool input_open_failed = false;
    std::string error;
};

} // namespace markov_cero::api
```

### 4.2 Comprehensive Engine Diagnostics Matrix
The following table details the exact telemetry emitted for each engine, status, and failure mode:

| Engine / Site | Condition / Status | `primal_residual` | `dual_residual` | `condition_estimate` | `failure_site` | `suggested_recovery` |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **QP** (`admm_solver`) | `optimal` | `qpres.primal_residual` | `qpres.dual_residual` | $\kappa(D) = \frac{\max \|D_k\|}{\min \|D_k\|}$ | `"none"` | `"none"` |
| **QP** (`admm_solver`) | `primal_infeasible` | `qpres.primal_residual` | `qpres.dual_residual` | $\kappa(D)$ | `"qp_primal_infeasibility_certificate"` | `"relax_incompatible_row_or_variable_bounds"` |
| **QP** (`admm_solver`) | `dual_infeasible` | `qpres.primal_residual` | `qpres.dual_residual` | $\kappa(D)$ | `"qp_dual_infeasibility_certificate"` | `"add_missing_variable_bounds_or_regularize_cost"` |
| **QP** (`admm_solver`) | `non_convex` | $0.0$ | $0.0$ | $\infty$ | `"qp_convexity_check"` | `"ensure_quadratic_objective_matrix_P_is_positive_semidefinite"` |
| **QP** (`admm_solver`) | `iteration_limit` | `qpres.primal_residual` | `qpres.dual_residual` | $\kappa(D)$ | `"qp_admm_iteration_limit"` | `"increase_iteration_limit_or_tune_rho_init"` |
| **QP** (`admm_solver`) | `numerical_error` | `qpres.primal_residual` | `qpres.dual_residual` | $\infty$ | `"qp_kkt_factorization"` | `"increase_regularization_sigma_or_rescale_problem"` |
| **PDLP** (`pdlp.cpp`) | `optimal` | `pdlp_res.primal_infeasibility` | `pdlp_res.dual_infeasibility` | $\max(\frac{\text{prim}}{\text{dual}}, \frac{\text{dual}}{\text{prim}})$ | `"none"` | `"none"` |
| **PDLP** (`pdlp.cpp`) | `iteration_limit` | `pdlp_res.primal_infeasibility` | `pdlp_res.dual_infeasibility` | $\max(\frac{\text{prim}}{\text{dual}}, \frac{\text{dual}}{\text{prim}})$ | `"pdlp_iteration_limit"` | `"crossover_to_dual_simplex_or_increase_iterations"` |
| **PDLP** (`pdlp.cpp`) | `numerical_error` | `pdlp_res.primal_infeasibility` | `pdlp_res.dual_infeasibility` | $\infty$ | `"pdlp_first_order_solver"` | `"crossover_to_dual_simplex_or_tighten_step_sizes"` |
| **Simplex / Dual** | `optimal` | `max_primal_viol` | `max_dual_viol` | `sparse_condition_estimate` | `"none"` | `"none"` |
| **Simplex / Dual** | `numerical_failure` | `max_primal_viol` | `max_dual_viol` | `sparse_condition_estimate` | `"dual_simplex_pivot"` | `"reinvert_basis_or_enable_ruiz_scaling"` |
| **Simplex / Presolve**| `infeasible` | $0.0$ | $0.0$ | $1.0$ | `"presolve_analysis"` | `"resolve_row_bound_contradiction_in_model"` |
| **Simplex / Presolve**| `unbounded` | $0.0$ | $0.0$ | $1.0$ | `"presolve_analysis"` | `"bound_free_primal_rays_in_model"` |
| **IPM** (`ipm.cpp`) | Uncertified fallback | `ipm_primal_residual` | `ipm_dual_residual` | $\kappa(A D A^T)$ | `"ipm_normal_equations_drift"` | `"dual_simplex_crossover_or_ruiz_scaling"` |
| **Verification** | Witness rejected | `canonical_primal_viol` | `canonical_dual_viol` | `basis_condition` | `"canonical_kkt_verification"` | `"refine_solution_or_lower_feasibility_tolerance"` |
| **Verification** | Original rejected | `primal_row_viol` | $0.0$ | $1.0$ | `"original_model_verification"` | `"check_presolve_postsolve_reconstruction"` |
| **MILP / Parallel** | `optimal` | `max_row_viol` | $0.0$ | $1.0$ | `"none"` | `"none"` |
| **MILP / Parallel** | `resource_limit` | `max_row_viol` | $0.0$ | $1.0$ | `"branch_and_bound_tree_search"` | `"increase_node_limit_or_time_limit"` |
| **IO / Parser** | `io::MpsError` | $0.0$ | $0.0$ | $0.0$ | `"mps_parser"` | `"correct_mps_syntax_at_indicated_record"` |
| **IO / Parser** | `cannot open input` | $0.0$ | $0.0$ | $0.0$ | `"file_io"` | `"verify_file_exists_and_has_read_permissions"` |
| **Exception Guard** | `std::length_error` | $0.0$ | $0.0$ | $0.0$ | `"memory_or_factor_limit"` | `"increase_maximum_factor_nonzeros"` |

### 4.3 Universal Safety Net in `finalize()`
To ensure that **no solve can ever emit an unpopulated or silent failure**, `finalize(SolveResult& out)` in `src/api/api.cpp` enforces:
```cpp
void finalize(SolveResult& out) {
    out.verified =
        (out.status == lp::reference::SolveStatus::optimal && out.original_verified &&
         out.canonical_verified) ||
        ((out.status == lp::reference::SolveStatus::infeasible ||
          out.status == lp::reference::SolveStatus::unbounded) &&
         out.canonical_verified);

    // Eliminate silent failures: guarantee valid diagnostic state
    if (out.diagnostic.failure_site.empty()) {
        if (out.status == lp::reference::SolveStatus::optimal) {
            out.diagnostic.failure_site = "none";
            out.diagnostic.suggested_recovery = "none";
        } else {
            out.diagnostic.failure_site = out.resolved_engine + "_solve";
            out.diagnostic.suggested_recovery = "inspect_engine_numerics_and_parameters";
        }
    }
    if (out.diagnostic.primal_residual == 0.0) {
        out.diagnostic.primal_residual = std::max(out.canonical_report.maximum_primal_violation,
                                                 out.primal_report.maximum_row_violation);
    }
    if (out.diagnostic.dual_residual == 0.0) {
        out.diagnostic.dual_residual = out.canonical_report.maximum_dual_violation;
    }
}
```

### 4.4 CLI JSON Serialization (`apps/json_output.hpp`)
In `apps/json_output.hpp`:
1. Add `markov_cero::api::NumericalDiagnostic diagnostic;` to `struct JsonOutputData`.
2. In `to_json_data()`, assign `data.diagnostic = res.diagnostic;`.
3. In `emit_json_output()`, append:
```cpp
    json << ",\"diagnostic\":{"
         << "\"primal_residual\":" << json_number(data.diagnostic.primal_residual) << ","
         << "\"dual_residual\":" << json_number(data.diagnostic.dual_residual) << ","
         << "\"condition_estimate\":" << json_number(data.diagnostic.condition_estimate) << ","
         << "\"failure_site\":\"" << json_escape(data.diagnostic.failure_site) << "\","
         << "\"suggested_recovery\":\"" << json_escape(data.diagnostic.suggested_recovery) << "\""
         << "}";
```
This produces structured telemetry in all JSON solve outputs without violating existing JSON syntax tests.

---

## 5. Verification Strategy & Invalidation Conditions

### 5.1 Independent Verification Commands
1. **CTest Full Suite**:
   ```bash
   ctest --test-dir build --output-on-failure
   ```
   *Requirement*: 59/59 passing (100% pass rate).
2. **QP Regression & Adaptation Verification**:
   ```bash
   ctest --test-dir build -R "^qp$" --verbose
   ```
   *Requirement*: KKT LDLᵀ factorization, unconstrained QP, constrained QP, non-convex rejection, primal infeasibility, MM synthetic benchmark, and MIQP all pass.
3. **Sparse Basis & Always-On Refinement Verification**:
   ```bash
   ctest --test-dir build -R "^sparse_basis$" --verbose
   ```
   *Requirement*: `small.solve()`, `moler.solve()`, `ill_on.solve()`, and `trig.solve()` all pass.
4. **API and CLI Diagnostic Verification**:
   ```bash
   ctest --test-dir build -R "^api_" --verbose
   ctest --test-dir build -R "^cli_" --verbose
   ```
   *Requirement*: `api_test`, `api_demo`, and all 9 `cli_*` tests pass cleanly.

### 5.2 Invalidation Conditions
The proposed implementation strategy is considered invalidated if:
1. `small.statistics().refinement_attempts != 0` on clean integer bases in `tests/sparse_basis_test.cpp`.
2. `sol.refactorization_count` fails to increment when $\rho$ updates on challenging QP instances.
3. Any non-optimal solve produces an empty `failure_site` or empty `suggested_recovery`.
4. Any CTest test in the 59-test baseline fails.
