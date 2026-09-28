#include "markov_cero/qp/verifier.hpp"
#include <algorithm>
#include <cmath>

namespace markov_cero::qp {
namespace {
bool finite(const std::vector<double>& v) {
    return std::all_of(v.begin(), v.end(), [](double x) { return std::isfinite(x); });
}
bool valid_tolerance(double t) { return std::isfinite(t) && t > 0 && t <= 1e-4; }
}
bool verify_qp_infeasibility(const QuadraticModel& model, const QpSolution& sol, double tol) {
    try {
        model.validate();
        const auto& y = sol.infeasibility_certificate;
        if (!valid_tolerance(tol) || y.size() != model.num_constraints() || !finite(y)) return false;
        long double support = 0.0L, support_scale = 0.0L;
        for (std::size_t i = 0; i < y.size(); ++i) {
            if (y[i] == 0) continue;
            const double bound = y[i] > 0 ? model.u[i] : model.l[i];
            if (!std::isfinite(bound)) return false;
            const long double term = static_cast<long double>(y[i]) * bound;
            support += term; support_scale += std::abs(term);
        }
        if (!std::isfinite(support) || !(support < -tol * std::max(1.0L, support_scale))) return false;
        for (std::size_t j = 0; j < model.num_variables(); ++j) {
            long double residual = 0, scale = 0;
            for (std::size_t k = model.A.column_offsets[j]; k < model.A.column_offsets[j + 1]; ++k) {
                const long double term = static_cast<long double>(model.A.values[k]) * y[model.A.row_indices[k]];
                residual += term; scale += std::abs(term);
            }
            if (!std::isfinite(residual) || std::abs(residual) > tol * std::max(1.0L, scale)) return false;
        }
        return true;
    } catch (...) { return false; }
}
bool verify_qp_unbounded(const QuadraticModel& model, const QpSolution& sol, double tol) {
    try {
        model.validate();
        const auto& d = sol.unbounded_ray;
        if (!valid_tolerance(tol) || d.size() != model.num_variables() ||
            sol.x.size() != d.size() || !finite(d) || !finite(sol.x) || !check_convexity(model.P)) return false;
        const auto pd = model.P.multiply(d);
        if (!finite(pd)) return false;
        long double slope = 0, slope_scale = 0;
        double norm = 0;
        for (std::size_t j = 0; j < d.size(); ++j) {
            norm = std::max(norm, std::abs(d[j]));
            const long double term = static_cast<long double>(model.q[j]) * d[j];
            slope += term; slope_scale += std::abs(term);
        }
        if (!(norm > 0) || !(slope < -tol * std::max(1.0L, slope_scale))) return false;
        for (double value : pd) if (std::abs(value) > tol * norm) return false;
        std::vector<long double> ax(model.num_constraints(), 0), ad(ax.size(), 0), scale(ax.size(), 1);
        for (std::size_t j = 0; j < d.size(); ++j)
            for (std::size_t k = model.A.column_offsets[j]; k < model.A.column_offsets[j + 1]; ++k) {
                const auto i = model.A.row_indices[k];
                const long double a = model.A.values[k];
                ax[i] += a * sol.x[j]; ad[i] += a * d[j]; scale[i] += std::abs(a * sol.x[j]);
            }
        for (std::size_t i = 0; i < ax.size(); ++i) {
            if (!std::isfinite(ax[i]) || !std::isfinite(ad[i])) return false;
            if (std::isfinite(model.l[i]) && (ax[i] < model.l[i] - tol * (scale[i] + std::abs(model.l[i])) || ad[i] < -tol * norm)) return false;
            if (std::isfinite(model.u[i]) && (ax[i] > model.u[i] + tol * (scale[i] + std::abs(model.u[i])) || ad[i] > tol * norm)) return false;
        }
        return true;
    } catch (...) { return false; }
}
} // namespace markov_cero::qp
