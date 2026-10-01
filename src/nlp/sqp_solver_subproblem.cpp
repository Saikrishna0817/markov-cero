#include "sqp_solver_internal.hpp"
namespace markov_cero::nlp {
using namespace detail_sqp_solver;
namespace detail_sqp_solver {
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
}

namespace detail_sqp_solver {
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
}

namespace detail_sqp_solver {
double dot_vectors(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) s += a[i] * b[i];
    return s;
}
}

namespace detail_sqp_solver {
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
}

namespace detail_sqp_solver {
double merit_directional_derivative(const NlpModel& model, const std::vector<double>& z,
                                    const std::vector<double>& d, double mu) {
    const auto grad = model.eval_gradient(z);
    if (model.has_callbacks()) {
        detail::note_callback_evaluation();
    }
    require_gradient(model, grad);
    std::size_t n_ineq = 0, n_eq = 0;
    const auto cvals = constraint_values(model, z, n_ineq, n_eq);
    const auto J = constraint_jacobian(model, z, n_ineq, n_eq);
    (void)n_eq;
    return merit_slope_from(grad, cvals, J, n_ineq, d, mu);
}
}

}
