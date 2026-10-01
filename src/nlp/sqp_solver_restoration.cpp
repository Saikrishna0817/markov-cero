#include "sqp_solver_internal.hpp"
namespace markov_cero::nlp {
using namespace detail_sqp_solver;
namespace detail_sqp_solver {
// NLP-02 contract nlp-restoration.md section 4: one bounded restoration
// attempt. Solve the slack-penalized QP, trust-cap the d step, backtrack
// until the ORIGINAL constraint violation drops by the documented factor
// (theta = 0.05) with a nonzero step. A rejection escalates rho by 10x
// (cap 1e8); slacks never enter the acceptance test.
RestorationOutcome attempt_restoration(const NlpModel& model, const SqpOptions& options,
                                       const std::vector<double>& x,
                                       const std::vector<double>& grad, const Lbfgs& lbfgs,
                                       const std::vector<std::vector<double>>& J,
                                       const std::vector<double>& cvals,
                                       std::size_t n_ineq, double mu, double& rho) {
    const std::size_t n = x.size();
    RestorationOutcome out;
    // Exact-penalty floor (section 3.3): rho dominates the merit penalty.
    rho = std::max({rho, mu, 10.0});

    qp::QuadraticModel sub =
        build_elastic_subproblem(model, x, grad, lbfgs, J, cvals, n_ineq, rho);
    sub.validate();
    const auto qp_sol = qp::solve_qp(sub, sqp_qp_options(options));
    if (qp_sol.status != qp::QpStatus::optimal || qp_sol.x.size() < n) {
        // The elastic QP is always feasible (section 3.1); any other
        // outcome is a rejected attempt, never a model claim.
        rho = std::min(rho * 10.0, 1e8);
        return out;
    }
    std::vector<double> d(qp_sol.x.begin(), qp_sol.x.begin() + static_cast<std::ptrdiff_t>(n));
    double d_norm = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        if (!std::isfinite(d[j])) {
            rho = std::min(rho * 10.0, 1e8);
            return out;
        }
        d_norm = std::max(d_norm, std::abs(d[j]));
    }
    if (d_norm > kTrustRadius) {
        const double shrink = kTrustRadius / d_norm;
        for (std::size_t j = 0; j < n; ++j) {
            d[j] *= shrink;
        }
    }

    const double viol0 = constraint_violation(model, x);
    constexpr double kAcceptDecrease = 0.05;  // theta (section 4.3)
    constexpr double kMinStep = 1e-10;        // A1 (section 4.3)
    double t = 1.0;
    for (int bt = 0; bt <= 10; ++bt, t *= 0.5) {
        std::vector<double> trial(n);
        double step_norm = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            trial[j] = x[j] + t * d[j];
            step_norm = std::max(step_norm, std::abs(t * d[j]));
        }
        if (step_norm < kMinStep) {
            break;  // A1: halving only shrinks a zero step further
        }
        projectToBounds(model, trial);
        const double viol1 = constraint_violation(model, trial);
        if (viol1 <= (1.0 - kAcceptDecrease) * viol0) {
            out.accepted = true;
            out.x_trial = std::move(trial);
            out.violation = viol1;
            return out;
        }
    }
    rho = std::min(rho * 10.0, 1e8);
    return out;
}
} // namespace detail_sqp_solver

namespace detail_sqp_solver {
// Section 5.2: the inconclusive exit names the mechanism and the counts;
// it never claims the NLP is infeasible.
std::string restoration_exhausted_message(const SqpSolution& sol,
                                          const SqpOptions& options) {
    std::string msg = "sqp: elastic restoration gave no original-violation decrease after ";
    msg += std::to_string(options.max_restoration_failures);
    msg += " consecutive rejected steps (";
    msg += std::to_string(sol.restoration_failures);
    msg += " rejected, ";
    msg += std::to_string(sol.restoration_steps);
    msg += " accepted); linearized infeasibility unresolved - inconclusive";
    return msg;
}
} // namespace detail_sqp_solver
} // namespace markov_cero::nlp
