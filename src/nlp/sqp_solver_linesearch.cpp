#include "sqp_solver_internal.hpp"
namespace markov_cero::nlp {
using namespace detail_sqp_solver;
namespace detail_sqp_solver {
// Trust cap + Armijo-Wolfe search, moved out of SqpSolver::solve verbatim
// under the NLP-01 line budget (contract nlp-local-sqp.md §4.2-4.3).
LineSearchOutcome line_search(const NlpModel& model, const SqpOptions& options,
                              const std::vector<double>& x, std::vector<double> d,
                              const std::vector<double>& grad,
                              const std::vector<double>& cvals,
                              const std::vector<std::vector<double>>& J,
                              std::size_t n_ineq, double mu) {
    const std::size_t n = x.size();
    // The QP subproblem includes the bound rows lb - x <= d <= ub - x, so at
    // a variable far from its bounds the ADMM direction d can equal the raw
    // -grad/scale (unbounded curvature proxy) and point to the OPPOSITE
    // bound. A trust-region cap on |d| keeps the SQP step local: this is the
    // standard bound-constrained SQP globalization (trust-region radius
    // scaled with the trust step), and the merit line search still
    // guarantees descent. WITHOUT it, t shrinks to ~1e-3 and progress stalls
    // on ill-conditioned objectives (Rosenbrock-class).
    // Shared constant (sqp_solver_internal.hpp), also used by the
    // NLP-02 restoration step.
    double d_norm = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        d_norm = std::max(d_norm, std::abs(d[j]));
    }
    if (d_norm > kTrustRadius) {
        const double shrink = kTrustRadius / d_norm;
        for (std::size_t j = 0; j < n; ++j) {
            d[j] *= shrink;
        }
    }
    const double obj_slope = dot_vectors(grad, d);
    const double slope = merit_slope_from(grad, cvals, J, n_ineq, d, mu);
    const double phi0 = merit_value(model, x, mu);
    double t = options.trust_step_scale;
    bool accepted = false;
    LineSearchOutcome out;
    out.x_trial.resize(n);
    std::vector<double> x_best(n);
    bool have_armijo_point = false;
    if (slope < 0.0) {
        // Descent validated: bracketed Wolfe search (Nocedal & Wright
        // 3.5/3.6 style). Armijo failures bound the bracket from above;
        // a curvature failure means the step is too short, so t expands
        // (bisecting toward the Armijo-failing bound once one exists).
        // The expansion cap keeps the step within 2x the trusted QP step;
        // when curvature is not reachable inside the cap the best
        // Armijo-satisfying step is accepted (documented waiver: the l1
        // merit is nonsmooth, so strict Wolfe is not always attainable).
        constexpr double kWolfeExpansionCap = 2.0;
        double t_lo = 0.0;   // largest step known to satisfy Armijo
        double t_hi = -1.0;  // smallest step known to violate Armijo
        for (int ls = 0; ls < 50; ++ls) {
            for (std::size_t j = 0; j < n; ++j) {
                out.x_trial[j] = x[j] + t * d[j];
            }
            projectToBounds(model, out.x_trial);
            if (merit_value(model, out.x_trial, mu) >
                phi0 + options.armijo_constant * t * slope) {
                t_hi = t;
                t = (t_lo > 0.0) ? 0.5 * (t_lo + t_hi) : 0.5 * t;
                continue;
            }
            // Armijo holds at t: remember it as the fallback candidate.
            t_lo = t;
            x_best = out.x_trial;
            have_armijo_point = true;
            const double slope_t =
                merit_directional_derivative(model, out.x_trial, d, mu);
            if (slope_t >= options.wolfe_curvature * slope) {
                accepted = true;
                break;
            }
            if (t_hi > 0.0) {
                t = 0.5 * (t_lo + t_hi);
            } else if (t >= kWolfeExpansionCap * options.trust_step_scale) {
                // Curvature not achievable within the cap; take the
                // Armijo-satisfying full step (waiver, see above).
                accepted = true;
                break;
            } else {
                t = std::min(2.0 * t, kWolfeExpansionCap * options.trust_step_scale);
            }
        }
        if (!accepted && have_armijo_point) {
            out.x_trial = x_best;
            accepted = true;
        }
    } else {
        // No merit descent direction (slope >= 0): the curvature condition
        // is meaningless, so keep the legacy Armijo-only safeguard with the
        // objective slope — behavior identical to the pre-Wolfe search.
        for (int ls = 0; ls < 50; ++ls) {
            for (std::size_t j = 0; j < n; ++j) {
                out.x_trial[j] = x[j] + t * d[j];
            }
            projectToBounds(model, out.x_trial);
            if (merit_value(model, out.x_trial, mu) <=
                phi0 + options.armijo_constant * t * obj_slope) {
                accepted = true;
                break;
            }
            t *= 0.5;
        }
    }
    out.accepted = accepted;
    return out;
}
} // namespace detail_sqp_solver
} // namespace markov_cero::nlp
