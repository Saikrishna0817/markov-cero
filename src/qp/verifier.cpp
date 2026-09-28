#include "markov_cero/qp/verifier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace markov_cero::qp {
namespace {
bool finite(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); });
}
}

QpVerificationReport verify_qp_solution(const QuadraticModel& model,
                                        const QpSolution& solution, double tolerance) {
    QpVerificationReport report;
    try {
        model.validate();
        const auto n = model.num_variables(), m = model.num_constraints();
        if (!std::isfinite(tolerance) || tolerance <= 0.0 || tolerance > 1e-4 ||
            solution.x.size() != n || solution.y.size() != m ||
            !finite(solution.x) || !finite(solution.y) ||
            !std::isfinite(solution.objective_value)) {
            report.failure_reason = "invalid tolerance, dimensions or non-finite QP witness";
            return report;
        }
        if (!check_convexity(model.P)) {
            report.failure_reason = "QP curvature is not certified positive semidefinite";
            return report;
        }
        std::vector<long double> ax(m, 0.0L), row_scale(m, 0.0L);
        std::vector<long double> aty(n, 0.0L), dual_scale(n, 0.0L);
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t k = model.A.column_offsets[j];
                 k < model.A.column_offsets[j + 1]; ++k) {
                const auto i = model.A.row_indices[k];
                const long double a = model.A.values[k];
                ax[i] += a * solution.x[j];
                row_scale[i] += std::abs(a * solution.x[j]);
                aty[j] += a * solution.y[i];
                dual_scale[j] += std::abs(a * solution.y[i]);
            }
        }
        bool primal_ok = true, dual_ok = true, comp_ok = true;
        for (std::size_t i = 0; i < m; ++i) {
            if (!std::isfinite(ax[i]) || !std::isfinite(row_scale[i])) {
                report.failure_reason = "QP row activity overflow";
                return report;
            }
            const double activity = static_cast<double>(ax[i]);
            for (bool lower : {true, false}) {
                const double bound = lower ? model.l[i] : model.u[i];
                if (!std::isfinite(bound)) continue;
                const double violation = std::max(0.0, lower ? bound - activity : activity - bound);
                report.maximum_primal_violation = std::max(report.maximum_primal_violation, violation);
                primal_ok = primal_ok && violation <= tolerance * (1.0L + row_scale[i] + std::abs(bound));
            }
            // y is the normal to [l,u]: positive selects u, negative selects l.
            const double y = solution.y[i];
            if (y == 0.0) continue;
            const double side = y > 0.0 ? model.u[i] : model.l[i];
            if (!std::isfinite(side)) {
                dual_ok = false;
                report.maximum_dual_violation = std::max(report.maximum_dual_violation, std::abs(y));
                continue;
            }
            const long double comp = std::abs(static_cast<long double>(y) * (ax[i] - side));
            const long double scale = 1.0L + std::abs(y) * (row_scale[i] + std::abs(side));
            report.maximum_complementarity_violation = std::max(
                report.maximum_complementarity_violation, static_cast<double>(comp));
            comp_ok = comp_ok && std::isfinite(comp) && comp <= tolerance * scale;
        }
        const auto px = model.P.multiply(solution.x);
        if (!finite(px)) {
            report.failure_reason = "QP gradient overflow";
            return report;
        }
        for (std::size_t j = 0; j < n; ++j) {
            const long double residual = std::abs(px[j] + static_cast<long double>(model.q[j]) + aty[j]);
            const long double scale = 1.0L + std::abs(px[j]) + std::abs(model.q[j]) + dual_scale[j];
            report.maximum_dual_violation = std::max(report.maximum_dual_violation, static_cast<double>(residual));
            dual_ok = dual_ok && std::isfinite(residual) && std::isfinite(scale) && residual <= tolerance * scale;
            if (!model.variable_types.empty() && model.variable_types[j] != model::VariableType::continuous)
                report.maximum_integrality_violation = std::max(report.maximum_integrality_violation,
                    std::abs(solution.x[j] - std::round(solution.x[j])));
        }
        long double internal = 0.0L;
        for (std::size_t j = 0; j < n; ++j)
            internal += 0.5L * solution.x[j] * px[j] + static_cast<long double>(model.q[j]) * solution.x[j];
        const long double objective = model.objective_offset +
            (model.sense == model::ObjectiveSense::maximize ? -internal : internal);
        report.objective_discrepancy = static_cast<double>(std::abs(objective - solution.objective_value));
        const bool objective_ok = std::isfinite(objective) &&
            std::isfinite(report.objective_discrepancy) &&
            report.objective_discrepancy <= tolerance * (1.0L + std::abs(objective));
        report.passed = primal_ok && dual_ok && comp_ok && objective_ok &&
                        report.maximum_integrality_violation <= tolerance;
        if (!report.passed) report.failure_reason =
            std::string("QP witness rejected:") + (primal_ok ? "" : " primal") +
            (dual_ok ? "" : " stationarity/sign") + (comp_ok ? "" : " complementarity") +
            (objective_ok ? "" : " objective") +
            (report.maximum_integrality_violation <= tolerance ? "" : " integrality");
    } catch (const std::exception& error) {
        report.failure_reason = std::string("QP verification failed: ") + error.what();
    }
    return report;
}
} // namespace markov_cero::qp
