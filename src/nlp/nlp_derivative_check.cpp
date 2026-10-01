#include "markov_cero/nlp/derivative_check.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace markov_cero::nlp {
namespace {

constexpr double kAbsTolerance = 1e-5;
constexpr double kRelTolerance = 1e-4;

bool within_tolerance(double analytic, double fd) {
    const double scale = std::max(std::abs(fd), std::abs(analytic));
    return std::abs(analytic - fd) <= kAbsTolerance + kRelTolerance * scale;
}

/// One-sided/centered finite difference of fn along coordinate j, honoring
/// finite bounds. Returns false when the coordinate has no room to step.
bool fd_along(const NlpModel& model, const std::vector<double>& x, std::size_t j,
              double hint, const std::function<double(const std::vector<double>&)>& fn,
              double& out) {
    const double xj = x[j];
    double h = (hint > 0.0 ? hint : std::cbrt(std::numeric_limits<double>::epsilon())) *
               std::max(1.0, std::abs(xj));
    const double room_lo = std::isfinite(model.bound_lower(j)) ? xj - model.bound_lower(j)
                                                               : std::numeric_limits<double>::infinity();
    const double room_hi = std::isfinite(model.bound_upper(j)) ? model.bound_upper(j) - xj
                                                               : std::numeric_limits<double>::infinity();
    if (!(room_lo > 0.0) && !(room_hi > 0.0)) {
        return false;  // fixed variable: no finite-difference direction
    }
    if (room_lo > 0.0 && room_hi > 0.0) {
        h = std::min(h, 0.5 * std::min(room_lo, room_hi));
        std::vector<double> plus = x, minus = x;
        plus[j] += h;
        minus[j] -= h;
        out = (fn(plus) - fn(minus)) / (2.0 * h);
        return true;
    }
    h = std::min(h, room_lo > 0.0 ? room_lo : room_hi);
    if (!(h > 0.0)) {
        return false;
    }
    std::vector<double> step = x;
    if (room_lo > 0.0) {  // at the upper bound: backward difference
        step[j] -= h;
        out = (fn(x) - fn(step)) / h;
    } else {  // at the lower bound: forward difference
        step[j] += h;
        out = (fn(step) - fn(x)) / h;
    }
    return true;
}

} // namespace

DerivativeReport check_derivatives(const NlpModel& model, const std::vector<double>& x,
                                    double step_hint) {
    DerivativeReport report;
    const double base = (step_hint > 0.0 ? step_hint
                                         : std::cbrt(std::numeric_limits<double>::epsilon()));
    report.base_step = base;
    if (x.size() != model.n_vars) {
        report.gradient_ok = false;
        report.jacobian_ok = false;
        report.message = "x dimension mismatch";
        return report;
    }
    for (double value : x) {
        if (!std::isfinite(value)) {
            report.gradient_ok = false;
            report.jacobian_ok = false;
            report.message = "x contains a non-finite value";
            return report;
        }
    }
    if (!model.has_callbacks() && model.poly_terms.empty()) {
        report.gradient_ok = false;
        report.jacobian_ok = false;
        report.message = "model has no objective to differentiate";
        return report;
    }

    try {
        // Gradient block.
        const auto analytic_grad = model.eval_gradient(x);
        if (analytic_grad.size() != model.n_vars) {
            report.gradient_ok = false;
            report.message = "gradient callback dimension mismatch";
            return report;
        }
        for (std::size_t j = 0; j < model.n_vars; ++j) {
            double fd = 0.0;
            if (!fd_along(model, x, j, step_hint,
                          [&](const std::vector<double>& z) { return model.eval_objective(z); },
                          fd)) {
                continue;
            }
            ++report.gradient_checks;
            const double err = std::abs(analytic_grad[j] - fd);
            report.worst_gradient_error = std::max(report.worst_gradient_error, err);
            if (!within_tolerance(analytic_grad[j], fd) ||
                !std::isfinite(analytic_grad[j]) || !std::isfinite(fd)) {
                report.gradient_ok = false;
            }
        }

        // Jacobian block: rows 0..n_ineq-1 from the inequality callbacks,
        // the rest from the equality callbacks (same order as
        // constraint_values).
        const auto check_rows = [&](const std::vector<std::vector<double>>& analytic,
                                    const std::function<std::vector<double>(
                                        const std::vector<double>&)>& values,
                                    std::size_t rows_expected, bool& ok,
                                    double& worst, std::size_t& checks) {
            if (rows_expected == 0) {
                return;
            }
            if (!values) {
                ok = false;
                return;
            }
            if (analytic.size() != rows_expected) {
                ok = false;
                return;
            }
            for (std::size_t i = 0; i < rows_expected; ++i) {
                if (analytic[i].size() != model.n_vars) {
                    ok = false;
                    return;
                }
                for (std::size_t j = 0; j < model.n_vars; ++j) {
                    double fd = 0.0;
                    const bool fine = fd_along(
                        model, x, j, step_hint,
                        [&](const std::vector<double>& z) { return values(z)[i]; }, fd);
                    if (!fine) {
                        continue;
                    }
                    ++checks;
                    const double err = std::abs(analytic[i][j] - fd);
                    worst = std::max(worst, err);
                    if (!within_tolerance(analytic[i][j], fd) ||
                        !std::isfinite(analytic[i][j]) || !std::isfinite(fd)) {
                        ok = false;
                    }
                }
            }
        };
        if (model.n_ineq > 0) {
            const auto analytic = model.has_ineq() ? model.ineq_jacobian(x)
                                                   : std::vector<std::vector<double>>{};
            check_rows(analytic, model.ineq_constraints, model.n_ineq, report.jacobian_ok,
                       report.worst_jacobian_error, report.jacobian_checks);
        }
        if (model.n_eq > 0) {
            const auto analytic = model.has_eq() ? model.eq_jacobian(x)
                                                 : std::vector<std::vector<double>>{};
            check_rows(analytic, model.eq_constraints, model.n_eq, report.jacobian_ok,
                       report.worst_jacobian_error, report.jacobian_checks);
        }
    } catch (const std::exception& e) {
        report.gradient_ok = false;
        report.jacobian_ok = false;
        report.message = std::string("callback raised during the diagnostic: ") + e.what();
        return report;
    } catch (...) {
        report.gradient_ok = false;
        report.jacobian_ok = false;
        report.message = "callback raised a non-standard exception during the diagnostic";
        return report;
    }

    if (report.message.empty()) {
        report.message = report.passed() ? "derivatives match finite differences"
                                         : "derivatives disagree with finite differences";
    }
    return report;
}

} // namespace markov_cero::nlp
