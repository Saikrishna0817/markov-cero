#include "sqp_solver_internal.hpp"
namespace markov_cero::nlp {
using namespace detail_sqp_solver;
SqpSolution SqpSolver::solve(const NlpModel& model, const std::vector<double>& x0) {
    const auto start_time = std::chrono::steady_clock::now();
    SqpSolution sol;
    model.validate();
    if (static_cast<std::size_t>(x0.size()) != model.n_vars) {
        sol.message = "x0 dimension mismatch";
        return sol;
    }

    const std::size_t n = model.n_vars;
    std::vector<double> x = x0;
    projectToBounds(model, x);

    Lbfgs lbfgs(static_cast<std::size_t>(options_.lbfgs_memory));
    double mu = options_.merit_penalty;

    std::vector<double> lam_ineq, lam_eq;
    std::vector<double> x_prev;

    auto finish = [&](lp::reference::SolveStatus status, std::string message) {
        sol.status = status;
        sol.message = std::move(message);
        sol.x = x;
        sol.objective = model.eval_objective(x);
        sol.ineq_multipliers = lam_ineq;
        sol.eq_multipliers = lam_eq;
        sol.constraint_violation = constraint_violation(model, x);
        // Stationarity residual with the multiplier estimates folded in,
        // projected onto the active bounds (bound multipliers absorb the
        // residual in fixed/bounded directions).
        std::vector<double> stat = model.eval_gradient(x);
        std::size_t n_ineq = 0, n_eq = 0;
        const auto J = constraint_jacobian(model, x, n_ineq, n_eq);
        for (std::size_t i = 0; i < n_ineq && i < lam_ineq.size(); ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                stat[j] += lam_ineq[i] * J[i][j];
            }
        }
        for (std::size_t i = 0; i < n_eq && i < lam_eq.size(); ++i) {
            const auto& row = J[n_ineq + i];
            for (std::size_t j = 0; j < n; ++j) {
                stat[j] += lam_eq[i] * row[j];
            }
        }
        std::size_t value_ineq = 0, value_eq = 0;
        const auto values = constraint_values(model, x, value_ineq, value_eq);
        double complementarity = 0.0;
        for (std::size_t i = 0; i < value_ineq && i < lam_ineq.size(); ++i) {
            complementarity = std::max(complementarity,
                                       std::abs(lam_ineq[i] * values[i]));
        }
        sol.kkt_residual = std::max({stationarity_violation(model, x, stat),
                                     sol.constraint_violation, complementarity});
        sol.solve_time_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time).count();
        return sol;
    };

    for (std::size_t iter = 0; iter < options_.max_iterations; ++iter) {
        if (options_.deadline && std::chrono::steady_clock::now() >= *options_.deadline) {
            return finish(lp::reference::SolveStatus::resource_limit,
                          "sqp wall-clock deadline reached");
        }
        sol.iterations = iter + 1;
        const auto grad = model.eval_gradient(x);
        std::size_t n_ineq = 0, n_eq = 0;
        const auto cvals = constraint_values(model, x, n_ineq, n_eq);
        const auto J = constraint_jacobian(model, x, n_ineq, n_eq);

        // QP subproblem uses the positive-definite limited-memory BFGS model.
        qp::QuadraticModel sub = build_subproblem(model, x, grad, lbfgs,
                                                  J, cvals, n_ineq);
        sub.validate();
        qp::QpOptions qp_opts;
        qp_opts.absolute_tolerance = 1e-8;
        qp_opts.relative_tolerance = 1e-8;
        qp_opts.max_iterations = 20000;
        qp_opts.time_limit_seconds = 10.0;
        qp_opts.deadline = options_.deadline;
        const auto qp_sol = qp::solve_qp(sub, qp_opts);

        if (qp_sol.status != qp::QpStatus::optimal || qp_sol.x.empty()) {
            // D-02 LOCKED fallback: indefinite B detected via the failed ADMM
            // solve -> reset Hessian, restart from the CURRENT point, at most
            // max_hessian_resets times.
            ++sol.hessian_resets;
            if (sol.hessian_resets > options_.max_hessian_resets) {
                return finish(lp::reference::SolveStatus::numerical_failure,
                              "sqp: QP subproblem failed after maximum hessian resets (" +
                                  std::string(qp::to_string(qp_sol.status)) + ")");
            }
            lbfgs.reset();
            mu = std::min(mu * 10.0, 1e8);  // escalate merit penalty once per reset
            continue;
        }

        // QP multipliers estimate the NLP multipliers (first-order).
        const std::size_t m = J.size();
        lam_ineq.assign(n_ineq, 0.0);
        lam_eq.assign(n_eq, 0.0);
        for (std::size_t i = 0; i < qp_sol.y.size(); ++i) {
            if (i < n_ineq) {
                lam_ineq[i] = std::max(0.0, qp_sol.y[i]);
            } else if (i < m) {
                lam_eq[i - n_ineq] = qp_sol.y[i];
            }
        }

        // Armijo-Wolfe line search on the l1 merit (feature 16, R3): both the
        // sufficient-decrease condition (c1) and the curvature condition (c2)
        // are enforced, with descent validation of the merit slope first.
        // The QP subproblem includes the bound rows lb - x <= d <= ub - x, so
        // at a variable far from its bounds the ADMM direction d can equal the
        // raw -grad/scale (unbounded curvature proxy) and point to the OPPOSITE
        // bound. A trust-region cap on |d| keeps the SQP step local: this is the
        // standard bound-constrained SQP globalization (trust-region radius
        // scaled with the trust step), and the merit line search below still
        // guarantees descent. WITHOUT it, t shrinks to ~1e-3 and progress
        // stalls on ill-conditioned objectives (Rosenbrock-class).
        std::vector<double> d = qp_sol.x;
        constexpr double kTrustRadius = 0.5;
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
        double t = options_.trust_step_scale;
        bool accepted = false;
        std::vector<double> x_trial(n);
        std::vector<double> x_best(n);
        bool have_armijo_point = false;
        double t_best = 0.0;
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
                    x_trial[j] = x[j] + t * d[j];
                }
                projectToBounds(model, x_trial);
                if (merit_value(model, x_trial, mu) >
                    phi0 + options_.armijo_constant * t * slope) {
                    t_hi = t;
                    t = (t_lo > 0.0) ? 0.5 * (t_lo + t_hi) : 0.5 * t;
                    continue;
                }
                // Armijo holds at t: remember it as the fallback candidate.
                t_lo = t;
                x_best = x_trial;
                t_best = t;
                have_armijo_point = true;
                const double slope_t = merit_directional_derivative(model, x_trial, d, mu);
                if (slope_t >= options_.wolfe_curvature * slope) {
                    accepted = true;
                    break;
                }
                if (t_hi > 0.0) {
                    t = 0.5 * (t_lo + t_hi);
                } else if (t >= kWolfeExpansionCap * options_.trust_step_scale) {
                    // Curvature not achievable within the cap; take the
                    // Armijo-satisfying full step (waiver, see above).
                    accepted = true;
                    break;
                } else {
                    t = std::min(2.0 * t, kWolfeExpansionCap * options_.trust_step_scale);
                }
            }
            if (!accepted && have_armijo_point) {
                x_trial = x_best;
                t = t_best;
                accepted = true;
            }
        } else {
            // No merit descent direction (slope >= 0): the curvature condition
            // is meaningless, so keep the legacy Armijo-only safeguard with the
            // objective slope — behavior identical to the pre-Wolfe search.
            for (int ls = 0; ls < 50; ++ls) {
                for (std::size_t j = 0; j < n; ++j) {
                    x_trial[j] = x[j] + t * d[j];
                }
                projectToBounds(model, x_trial);
                if (merit_value(model, x_trial, mu) <=
                    phi0 + options_.armijo_constant * t * obj_slope) {
                    accepted = true;
                    break;
                }
                t *= 0.5;
            }
        }
        if (!accepted) {
            ++sol.hessian_resets;
            if (sol.hessian_resets > options_.max_hessian_resets) {
                return finish(lp::reference::SolveStatus::numerical_failure,
                              "sqp: line search failed after maximum hessian resets");
            }
            lbfgs.reset();
            mu = std::min(mu * 10.0, 1e8);
            continue;
        }

        // Update curvature from the accepted step; the next QP receives the
        // resulting rank-two BFGS updates through the limited-memory history.
        std::vector<double> s(n), y(n);
        const auto grad_new = model.eval_gradient(x_trial);
        for (std::size_t j = 0; j < n; ++j) {
            s[j] = x_trial[j] - x[j];
            y[j] = grad_new[j] - grad[j];
        }
        lbfgs.update(s, y);
        x = x_trial;
        x_prev = x;

        // Convergence: KKT residual <= 1e-6 (LOCKED). Feasibility dominates;
        // stationarity uses the QP multipliers with bound absorption.
        const double viol_new = constraint_violation(model, x);
        if (viol_new < options_.kkt_tolerance) {
            const auto grad_here = model.eval_gradient(x);
            std::vector<double> stat = grad_here;
            std::size_t ni = 0, ne = 0;
            const auto Jh = constraint_jacobian(model, x, ni, ne);
            for (std::size_t i = 0; i < ni && i < lam_ineq.size(); ++i) {
                for (std::size_t j = 0; j < n; ++j) {
                    stat[j] += lam_ineq[i] * Jh[i][j];
                }
            }
            for (std::size_t i = 0; i < ne && i < lam_eq.size(); ++i) {
                const auto& row = Jh[ni + i];
                for (std::size_t j = 0; j < n; ++j) {
                    stat[j] += lam_eq[i] * row[j];
                }
            }
            double complementarity = 0.0;
            std::size_t ni_values = 0, ne_values = 0;
            const auto values_here = constraint_values(model, x, ni_values, ne_values);
            if (ni != ni_values || ne != ne_values || values_here.size() != ni + ne) {
                return finish(lp::reference::SolveStatus::numerical_failure,
                              "sqp: constraint callback dimensions changed during solve");
            }
            for (std::size_t i = 0; i < ni && i < lam_ineq.size(); ++i) {
                complementarity = std::max(complementarity,
                                           std::abs(lam_ineq[i] * values_here[i]));
            }
            if (stationarity_violation(model, x, stat) < options_.kkt_tolerance &&
                complementarity < options_.kkt_tolerance) {
                return finish(lp::reference::SolveStatus::optimal,
                              "sqp: primal, stationarity, and complementarity KKT residuals satisfied");
            }
        }
        (void)m;
    }

    return finish(lp::reference::SolveStatus::iteration_limit,
                  "sqp: maximum iterations reached");
}
SqpSolution solve_sqp(const NlpModel& model, const std::vector<double>& x0,
                      const SqpOptions& options) {
    SqpSolver solver(options);
    return solver.solve(model, x0);
}
}
