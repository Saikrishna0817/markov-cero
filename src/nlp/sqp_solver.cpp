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
    for (double value : x0) {
        if (!std::isfinite(value)) {
            sol.message = "x0 contains a non-finite value";
            return sol;
        }
    }

    const std::size_t n = model.n_vars;
    detail::reset_callback_evaluations();
    std::vector<double> x = x0;
    projectToBounds(model, x);
    // Contract §4.5: disclose how far x0 was moved onto the bounds.
    for (std::size_t j = 0; j < n; ++j) {
        sol.x0_projection_norm =
            std::max(sol.x0_projection_norm, std::abs(x[j] - x0[j]));
    }
    sol.x0_projected = sol.x0_projection_norm > 0.0;

    Lbfgs lbfgs(static_cast<std::size_t>(options_.lbfgs_memory));
    double mu = options_.merit_penalty;
    std::vector<double> lam_ineq, lam_eq;

    // Contract §5.4: remember the last iterate inside the feasibility
    // tolerance; the engine attaches it (after its own verifier) on
    // non-KKT exits without upgrading the status.
    const auto remember_feasible = [&](const std::vector<double>& point, double violation) {
        if (std::isfinite(violation) && violation <= options_.kkt_tolerance) {
            sol.best_feasible_x = point;
            sol.best_feasible_objective = model.eval_objective(point);
            if (model.has_callbacks()) {
                detail::note_callback_evaluation();
            }
        }
    };

    auto finish = [&](lp::reference::SolveStatus status, std::string message) {
        sol.status = status;
        sol.message = std::move(message);
        sol.x = x;
        sol.ineq_multipliers = lam_ineq;
        sol.eq_multipliers = lam_eq;
        sol.solve_time_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time).count();
        try {
            sol.objective = model.eval_objective(x);
            if (model.has_callbacks()) {
                detail::note_callback_evaluation();
            }
            require_objective(sol.objective);
            sol.constraint_violation = constraint_violation(model, x);
            // Stationarity residual with the multiplier estimates folded in,
            // projected onto the active bounds (bound multipliers absorb the
            // residual in fixed/bounded directions).
            std::vector<double> stat = model.eval_gradient(x);
            if (model.has_callbacks()) {
                detail::note_callback_evaluation();
            }
            require_gradient(model, stat);
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
        } catch (const std::exception& e) {
            // Contract §2.6: a callback broken at report time fails closed
            // to unknown (JSON null), never to a plausible-looking zero.
            sol.objective = std::numeric_limits<double>::quiet_NaN();
            sol.constraint_violation = std::numeric_limits<double>::quiet_NaN();
            sol.kkt_residual = std::numeric_limits<double>::quiet_NaN();
            sol.message += "; reporting after failed callback evaluation: ";
            sol.message += e.what();
        } catch (...) {
            sol.objective = std::numeric_limits<double>::quiet_NaN();
            sol.constraint_violation = std::numeric_limits<double>::quiet_NaN();
            sol.kkt_residual = std::numeric_limits<double>::quiet_NaN();
            sol.message +=
                "; reporting after failed callback evaluation (non-standard exception)";
        }
        sol.callback_evaluations = detail::take_callback_evaluations();
        return sol;
    };

    try {
        remember_feasible(x, constraint_violation(model, x));

        for (std::size_t iter = 0; iter < options_.max_iterations; ++iter) {
            if (options_.deadline &&
                std::chrono::steady_clock::now() >= *options_.deadline) {
                return finish(lp::reference::SolveStatus::resource_limit,
                              "sqp wall-clock deadline reached");
            }
            sol.iterations = iter + 1;
            const auto grad = model.eval_gradient(x);
            if (model.has_callbacks()) {
                detail::note_callback_evaluation();
            }
            require_gradient(model, grad);
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

            if (qp_sol.status == qp::QpStatus::primal_infeasible) {
                // Contract §4.4: B_k affects only the objective, so a
                // primal-infeasible linearization cannot be repaired by a
                // Hessian reset — fail immediately and inconclusively
                // (never Infeasible, never a claim about the NLP itself).
                return finish(
                    lp::reference::SolveStatus::numerical_failure,
                    "sqp: QP subproblem linearization is primal-infeasible at the "
                    "current iterate; elastic restoration is deferred (NLP-02)");
            }
            if (qp_sol.status != qp::QpStatus::optimal || qp_sol.x.empty()) {
                // D-02 LOCKED fallback: indefinite B detected via the failed
                // ADMM solve -> reset Hessian, restart from the CURRENT
                // point, at most max_hessian_resets times.
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

            // Contract §4.2-4.3: trust-cap the step, then the bracketed
            // Armijo-Wolfe search on the l1 merit (Wolfe waiver inside the
            // 2x expansion cap), extracted into line_search().
            const auto ls = line_search(model, options_, x, qp_sol.x, grad,
                                        cvals, J, n_ineq, mu);
            if (!ls.accepted) {
                ++sol.hessian_resets;
                if (sol.hessian_resets > options_.max_hessian_resets) {
                    return finish(lp::reference::SolveStatus::numerical_failure,
                                  "sqp: line search failed after maximum hessian resets");
                }
                lbfgs.reset();
                mu = std::min(mu * 10.0, 1e8);
                continue;
            }

            // Update curvature from the accepted step; the next QP receives
            // the resulting rank-two BFGS updates through the L-BFGS history.
            std::vector<double> s(n), yv(n);
            const auto grad_new = model.eval_gradient(ls.x_trial);
            if (model.has_callbacks()) {
                detail::note_callback_evaluation();
            }
            require_gradient(model, grad_new);
            for (std::size_t j = 0; j < n; ++j) {
                s[j] = ls.x_trial[j] - x[j];
                yv[j] = grad_new[j] - grad[j];
            }
            lbfgs.update(s, yv);
            x = ls.x_trial;

            // Convergence: KKT residual <= 1e-6 (LOCKED). Feasibility
            // dominates; stationarity uses the QP multipliers with bound
            // absorption. The same feasibility sample feeds §5.4.
            const double viol_new = constraint_violation(model, x);
            remember_feasible(x, viol_new);
            if (viol_new < options_.kkt_tolerance) {
                const auto grad_here = model.eval_gradient(x);
                if (model.has_callbacks()) {
                    detail::note_callback_evaluation();
                }
                require_gradient(model, grad_here);
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
                for (std::size_t i = 0; i < ni && i < lam_ineq.size(); ++i) {
                    complementarity = std::max(complementarity,
                                               std::abs(lam_ineq[i] * values_here[i]));
                }
                if (stationarity_violation(model, x, stat) < options_.kkt_tolerance &&
                    complementarity < options_.kkt_tolerance) {
                    return finish(lp::reference::SolveStatus::optimal,
                                  "sqp: primal, stationarity, and complementarity KKT "
                                  "residuals satisfied");
                }
            }
        }
        return finish(lp::reference::SolveStatus::iteration_limit,
                      "sqp: maximum iterations reached");
    } catch (const std::exception& e) {
        return finish(lp::reference::SolveStatus::numerical_failure,
                      std::string("sqp: evaluation failed: ") + e.what());
    } catch (...) {
        return finish(lp::reference::SolveStatus::numerical_failure,
                      "sqp: evaluation failed with a non-standard exception");
    }
}
SqpSolution solve_sqp(const NlpModel& model, const std::vector<double>& x0,
                      const SqpOptions& options) {
    SqpSolver solver(options);
    return solver.solve(model, x0);
}
}
