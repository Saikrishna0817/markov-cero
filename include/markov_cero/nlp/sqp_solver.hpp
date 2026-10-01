#pragma once

#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/nlp/nlp_model.hpp"

#include <cstddef>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace markov_cero::nlp {

// W1 / D-02 (LOCKED): SQP with L-BFGS-B Hessian approximation.
//
//   minimize    f(x)
//   subject to  g(x) <= 0,  h(x) = 0,  lb <= x <= ub
//
// Each major iteration solves a strictly convex QP subproblem
//   minimize  1/2 d^T B_k d + grad f(x_k)^T d
//   s.t.      g(x_k) + J_g(x_k) d <= 0
//             h(x_k) + J_h(x_k) d  = 0
//             lb - x_k <= d <= ub - x_k
// with B_k the (positive definite) limited-memory BFGS approximation, then
// performs a strong-Wolfe line search on the l1 merit function
//   phi(x; mu) = f(x) + mu * [ sum_i max(0, g_i(x)) + sum_j |h_j(x)| ].
//
// Fallback (LOCKED): if the QP subproblem fails (indefinite B_k detected via
// the failed ADMM solve), B_k resets to identity and the iteration restarts
// from the CURRENT point (not x0); at most 3 resets before numerical_failure.

struct SqpOptions {
    std::size_t max_iterations{200};
    double kkt_tolerance{1e-6};          // LOCKED convergence threshold
    double armijo_constant{1e-4};        // c1 (LOCKED)
    double wolfe_curvature{0.9};         // c2 (LOCKED)
    std::size_t lbfgs_memory{10};        // m = 10 pairs (LOCKED)
    std::size_t max_hessian_resets{3};   // LOCKED fallback budget
    double merit_penalty{10.0};          // initial mu for the l1 merit function
    double trust_step_scale{1.0};        // scales the QP step before line search
    bool verbose{false};
    std::optional<std::chrono::steady_clock::time_point> deadline;
    // NLP-02 contract nlp-restoration.md: elastic restoration on a
    // primal-infeasible linearized QP. false = the NLP-01 immediate
    // inconclusive failure (used by the paired benchmark as the old path).
    bool elastic_restoration{true};
    // NLP-02 contract nlp-restoration.md section 5: consecutive rejected
    // restoration attempts (without an accepted step) before an
    // inconclusive exit.
    std::size_t max_restoration_failures{5};
};

struct SqpSolution {
    lp::reference::SolveStatus status{lp::reference::SolveStatus::numerical_failure};
    std::string message;
    std::vector<double> x;
    double objective{0.0};
    // KKT residual: ||grad f + J_g^T lambda_g + J_h^T lambda_h - z||_inf
    // combined with max constraint violation, both at the returned point.
    double kkt_residual{0.0};
    double constraint_violation{0.0};
    std::size_t iterations{0};
    std::size_t hessian_resets{0};
    double solve_time_seconds{0.0};
    // Multipliers for telemetry (ineq then eq).
    std::vector<double> ineq_multipliers;
    std::vector<double> eq_multipliers;
    // NLP-01 contract nlp-local-sqp.md §2.4/§4.5/§5.4: user-callback
    // invocations during this solve, the bound projection applied to x0,
    // and the last feasibility-tolerated iterate kept for non-KKT exits
    // (empty when no iterate met the feasibility tolerance).
    std::size_t callback_evaluations{0};
    bool x0_projected{false};
    double x0_projection_norm{0.0};
    std::vector<double> best_feasible_x;
    double best_feasible_objective{0.0};
    // NLP-02 contract nlp-restoration.md sections 4-6: accepted elastic
    // restoration steps, rejected restoration attempts, and whether the
    // consecutive-failure budget ended the solve (inconclusive).
    std::size_t restoration_steps{0};
    std::size_t restoration_failures{0};
    bool restoration_exhausted{false};
};

class SqpSolver {
  public:
    explicit SqpSolver(SqpOptions options = {}) : options_(options) {}

    // Callback path (D-01 Path A).
    SqpSolution solve(const NlpModel& model, const std::vector<double>& x0);

  private:
    SqpOptions options_;
};

// Free-function convenience wrapper.
SqpSolution solve_sqp(const NlpModel& model, const std::vector<double>& x0,
                      const SqpOptions& options = {});

} // namespace markov_cero::nlp
