#include "admm_solver_internal.hpp"
#include "markov_cero/qp/verifier.hpp"
namespace markov_cero::qp {
using namespace detail_admm_solver;
namespace detail_admm_solver {
void verify_accepted_result(const QuadraticModel& model, QpSolution& sol) {
    // The zero-variable public shorthand uses empty CSC offsets; normalize
    // only the verifier's view, as the high-level solve path does for scaling.
    QuadraticModel normalized;
    const QuadraticModel* checked = &model;
    if (model.num_variables() == 0 &&
        (model.P.column_offsets.empty() || model.A.column_offsets.empty())) {
        normalized = model;
        if (normalized.P.column_offsets.empty()) normalized.P.column_offsets = {0};
        if (normalized.A.column_offsets.empty()) normalized.A.column_offsets = {0};
        checked = &normalized;
    }
    bool accepted = false;
    switch (sol.status) {
    case QpStatus::optimal:
        accepted = verify_qp_solution(*checked, sol).passed;
        break;
    case QpStatus::primal_infeasible:
        accepted = verify_qp_infeasibility(*checked, sol, 1e-4);
        break;
    case QpStatus::dual_infeasible:
        accepted = verify_qp_unbounded(*checked, sol, 1e-4);
        break;
    default:
        sol.verified = false;
        return;
    }
    sol.verified = accepted;
    if (!accepted) {
        sol.status = QpStatus::numerical_error;
        sol.message = "QP accepted witness rejected by independent verification";
    }
}
}
const char* to_string(QpStatus status) noexcept {
    switch (status) {
    case QpStatus::optimal:
        return "optimal";
    case QpStatus::primal_infeasible:
        return "primal_infeasible";
    case QpStatus::dual_infeasible:
        return "dual_infeasible";
    case QpStatus::iteration_limit:
        return "iteration_limit";
    case QpStatus::time_limit:
        return "time_limit";
    case QpStatus::non_convex:
        return "non_convex";
    case QpStatus::unsupported:
        return "unsupported";
    case QpStatus::numerical_error:
        return "numerical_error";
    }
    return "unknown";
}
namespace detail_admm_solver {
double inf_norm(const std::vector<double>& v) noexcept {
    double max_val = 0.0;
    for (double x : v) {
        max_val = std::max(max_val, std::abs(x));
    }
    return max_val;
}
}

namespace detail_admm_solver {
std::vector<double> multiply_A(const linalg::SparseCsc& A,
                               const std::vector<double>& x) {
    std::vector<double> y(A.rows, 0.0);
    for (std::size_t j = 0; j < A.columns; ++j) {
        const double xj = x[j];
        if (xj == 0.0) {
            continue;
        }
        const std::size_t start = A.column_offsets[j];
        const std::size_t end = A.column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            y[A.row_indices[k]] += A.values[k] * xj;
        }
    }
    return y;
}
}

namespace detail_admm_solver {
std::vector<double> multiply_AT(const linalg::SparseCsc& A,
                                const std::vector<double>& y) {
    std::vector<double> x(A.columns, 0.0);
    for (std::size_t j = 0; j < A.columns; ++j) {
        double sum = 0.0;
        const std::size_t start = A.column_offsets[j];
        const std::size_t end = A.column_offsets[j + 1];
        for (std::size_t k = start; k < end; ++k) {
            sum += A.values[k] * y[A.row_indices[k]];
        }
        x[j] = sum;
    }
    return x;
}
}

namespace detail_admm_solver {
double project_bound(double v, double l, double u) noexcept {
    if (v < l) {
        return l;
    }
    if (v > u) {
        return u;
    }
    return v;
}
}

namespace detail_admm_solver {
bool infeasibility_certificate(const QuadraticModel& model, const QpOptions& options,
    const std::vector<double>& x, const std::vector<double>& y,
    const std::vector<double>& x_prev, const std::vector<double>& y_prev, QpSolution& sol) {
    const auto n=model.num_variables(), m=model.num_constraints();
            // Primal infeasibility check
            std::vector<double> delta_y(m, 0.0);
            for (std::size_t i = 0; i < m; ++i) {
                delta_y[i] = y[i] - y_prev[i];
            }
            const double norm_dy = inf_norm(delta_y);
            if (norm_dy > 1e-10) {
                const std::vector<double> ATdy = multiply_AT(model.A, delta_y);
                if (inf_norm(ATdy) <=
                    options.primal_infeasible_tolerance * norm_dy) {
                    double support = 0.0;
                    bool has_infinite_ray = false;
                    for (std::size_t i = 0; i < m; ++i) {
                        if (delta_y[i] > 1e-12) {
                            if (std::isinf(model.u[i])) {
                                has_infinite_ray = true;
                                break;
                            }
                            support += model.u[i] * delta_y[i];
                        } else if (delta_y[i] < -1e-12) {
                            if (std::isinf(model.l[i])) {
                                has_infinite_ray = true;
                                break;
                            }
                            support += model.l[i] * delta_y[i];
                        }
                    }
                    if (!has_infinite_ray &&
                        support <
                            -options.primal_infeasible_tolerance * norm_dy) {
                        sol.status = QpStatus::primal_infeasible;
                        sol.infeasibility_certificate = delta_y;
                        return true;
                    }
                }
            }

            // Dual infeasibility (unboundedness) check
            std::vector<double> delta_x(n, 0.0);
            for (std::size_t j = 0; j < n; ++j) {
                delta_x[j] = x[j] - x_prev[j];
            }
            const double norm_dx = inf_norm(delta_x);
            if (norm_dx > 1e-10) {
                const std::vector<double> Pdx = model.P.multiply(delta_x);
                double q_dot_dx = 0.0;
                for (std::size_t j = 0; j < n; ++j) {
                    q_dot_dx += model.q[j] * delta_x[j];
                }
                if (inf_norm(Pdx) <=
                        options.dual_infeasible_tolerance * norm_dx &&
                    q_dot_dx < -options.dual_infeasible_tolerance * norm_dx) {
                    const std::vector<double> Adx = multiply_A(model.A, delta_x);
                    bool ray_compatible = true;
                    for (std::size_t i = 0; i < m; ++i) {
                        if (Adx[i] > options.dual_infeasible_tolerance * norm_dx &&
                            !std::isinf(model.u[i])) {
                            ray_compatible = false;
                            break;
                        }
                        if (Adx[i] < -options.dual_infeasible_tolerance * norm_dx &&
                            !std::isinf(model.l[i])) {
                            ray_compatible = false;
                            break;
                        }
                    }
                    if (ray_compatible) {
                        sol.status = QpStatus::dual_infeasible;
                        sol.unbounded_ray = delta_x;
                        return true;
                    }
                }
            }
    return false;
}
}

}
