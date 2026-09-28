#include "markov_cero/nlp/sqp_solver.hpp"

#include "markov_cero/nlp/lbfgs.hpp"
#include "markov_cero/qp/admm_solver.hpp"
#include "nlp_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace markov_cero::nlp {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// Bound-aware stationarity violation (projected-gradient KKT):
//   interior variables require stat_j == 0;
//   at a lower (upper) bound a positive (negative) residual is absorbed by
//   the bound multiplier;
//   fixed variables (lb == ub) have a free-sign multiplier (any residual OK).
double stationarity_violation(const NlpModel& model, const std::vector<double>& x,
                              const std::vector<double>& stat) {
    double worst = 0.0;
    for (std::size_t j = 0; j < stat.size(); ++j) {
        const double lb = model.bound_lower(j);
        const double ub = model.bound_upper(j);
        double v;
        if (std::isfinite(lb) && std::isfinite(ub) && std::abs(lb - ub) <= 1e-12) {
            v = 0.0;  // fixed variable: free-sign multiplier
        } else if (std::isfinite(lb) && x[j] - lb <= 1e-9) {
            v = std::max(0.0, -stat[j]);  // at lower bound: r >= 0 absorbable
        } else if (std::isfinite(ub) && ub - x[j] <= 1e-9) {
            v = std::max(0.0, stat[j]);   // at upper bound: r <= 0 absorbable
        } else {
            v = std::abs(stat[j]);
        }
        worst = std::max(worst, v);
    }
    return worst;
}

// Assemble the QP subproblem at x_k (D-02):
//   min 1/2 d^T B d + grad^T d
//   s.t. J d <= -cvals[i]   (ineq, i < n_ineq)
//        J d  = -cvals[i]   (eq)
//        lb - x <= d <= ub - x   (variable bounds as rows)
// B is the positive-definite limited-memory BFGS Hessian built from accepted
// gradient-difference pairs; variable bounds remain explicit QP constraints.
qp::QuadraticModel build_subproblem(const NlpModel& model,
                                    const std::vector<double>& x,
                                    const std::vector<double>& grad,
                                    const Lbfgs& lbfgs,
                                    const std::vector<std::vector<double>>& J,
                                    const std::vector<double>& cvals,
                                    std::size_t n_ineq) {
    const std::size_t n = x.size();
    const std::size_t m = J.size();
    const std::size_t rows_total = m + n;  // constraints + bound rows

    qp::QuadraticModel qm;
    qm.name = "sqp_subproblem";
    qm.sense = model::ObjectiveSense::minimize;

    // Use the retained limited-memory BFGS curvature in the QP objective.
    const auto B = lbfgs.hessian_matrix(n);
    qm.P.dimension = n;
    qm.P.column_offsets.assign(n + 1, 0);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i <= j; ++i) {
            const double value = B[i * n + j];
            if (value != 0.0) {
                qm.P.row_indices.push_back(i);
                qm.P.values.push_back(value);
            }
        }
        qm.P.column_offsets[j + 1] = qm.P.values.size();
    }
    qm.q = grad;

    // Rows: nonlinear constraints (J), then bound rows (e_j).
    std::vector<std::vector<double>> rows;
    rows.reserve(rows_total);
    for (std::size_t i = 0; i < m; ++i) {
        rows.push_back(J[i]);
    }
    std::vector<double> lo(rows_total, -kInf);
    std::vector<double> up(rows_total, kInf);
    for (std::size_t i = 0; i < m; ++i) {
        if (i < n_ineq) {
            up[i] = -cvals[i];            // J d <= -g(x)
        } else {
            lo[i] = -cvals[i];            // J d = -h(x)
            up[i] = -cvals[i];
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        std::vector<double> e(n, 0.0);
        e[j] = 1.0;
        rows.push_back(e);
        const double lb = model.bound_lower(j);
        const double ub = model.bound_upper(j);
        lo[m + j] = std::isfinite(lb) ? lb - x[j] : -kInf;
        up[m + j] = std::isfinite(ub) ? ub - x[j] : kInf;
    }

    // Column-major CSC of the (rows_total x n) matrix.
    qm.A.rows = rows_total;
    qm.A.columns = n;
    qm.A.column_offsets.assign(n + 1, 0);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < rows_total; ++i) {
            if (rows[i][j] != 0.0) {
                qm.A.row_indices.push_back(i);
                qm.A.values.push_back(rows[i][j]);
            }
        }
        qm.A.column_offsets[j + 1] = qm.A.values.size();
    }
    qm.l = std::move(lo);
    qm.u = std::move(up);
    qm.variable_names.resize(n);
    for (std::size_t j = 0; j < n; ++j) {
        qm.variable_names[j] = "d" + std::to_string(j);
    }
    qm.constraint_names.resize(rows_total);
    for (std::size_t i = 0; i < rows_total; ++i) {
        qm.constraint_names[i] = "r" + std::to_string(i);
    }
    return qm;
}

} // namespace

// --- Armijo-Wolfe line search support (feature 16 / R3) ---------------------
namespace {

constexpr double kActiveTolerance = 1e-10;

double dot_vectors(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) s += a[i] * b[i];
    return s;
}

// Directional derivative of the l1 merit function along d at a point whose
// objective gradient, constraint values and Jacobian are already in hand.
// The l1 merit is nonsmooth where a constraint is active, so the correct
// one-sided (Bouligand) derivative is used there:
//   g_i > tol :  mu * J_i d          (penalty active)
//   |g_i|<=tol : mu * max(0, J_i d)  (nonsmooth kink, descent-aware)
//   g_i < -tol:  0
//   |h_j|>tol :  mu * sign(h_j) * J_j d
//   |h_j|<=tol :  mu * |J_j d|       (nonsmooth kink of |h|)
double merit_slope_from(const std::vector<double>& grad,
                        const std::vector<double>& cvals,
                        const std::vector<std::vector<double>>& J,
                        std::size_t n_ineq, const std::vector<double>& d, double mu) {
    double deriv = dot_vectors(grad, d);
    for (std::size_t i = 0; i < n_ineq; ++i) {
        const double jd = dot_vectors(J[i], d);
        if (cvals[i] > kActiveTolerance)
            deriv += mu * jd;
        else if (cvals[i] >= -kActiveTolerance)
            deriv += mu * std::max(0.0, jd);
    }
    for (std::size_t i = n_ineq; i < cvals.size(); ++i) {
        const double jd = dot_vectors(J[i], d);
        deriv += std::abs(cvals[i]) > kActiveTolerance
                     ? mu * (cvals[i] > 0.0 ? jd : -jd)
                     : mu * std::abs(jd);
    }
    return deriv;
}

// Directional derivative of the l1 merit at an arbitrary trial point (the
// gradient/Jacobian callbacks are re-evaluated there).
double merit_directional_derivative(const NlpModel& model, const std::vector<double>& z,
                                    const std::vector<double>& d, double mu) {
    const auto grad = model.eval_gradient(z);
    std::size_t n_ineq = 0, n_eq = 0;
    const auto cvals = constraint_values(model, z, n_ineq, n_eq);
    const auto J = constraint_jacobian(model, z, n_ineq, n_eq);
    (void)n_eq;
    return merit_slope_from(grad, cvals, J, n_ineq, d, mu);
}

} // namespace

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

} // namespace markov_cero::nlp
