#include "markov_cero/nlp/nlp_verifier.hpp"

#include "nlp_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace markov_cero::nlp {

NlpFeasibilityReport verify_nlp_feasibility(const NlpModel& model,
                                            const std::vector<double>& x,
                                            double tolerance) {
    NlpFeasibilityReport report;
    const auto reject = [&report](const std::string& why) {
        report.feasible = false;
        report.message = why;
        report.maximum_violation = std::numeric_limits<double>::infinity();
        report.maximum_constraint_violation = std::numeric_limits<double>::infinity();
        report.maximum_bound_violation = std::numeric_limits<double>::infinity();
        return report;
    };
    if (!std::isfinite(tolerance) || tolerance < 0.0) {
        return reject("invalid feasibility tolerance");
    }
    if (x.size() != model.n_vars) {
        return reject("primal dimension mismatch");
    }
    if ((model.n_ineq > 0 && (!model.ineq_constraints || !model.ineq_jacobian)) ||
        (model.n_eq > 0 && (!model.eq_constraints || !model.eq_jacobian))) {
        return reject("constraint callback missing");
    }
    for (double value : x) {
        if (!std::isfinite(value)) {
            return reject("primal contains a non-finite value");
        }
    }

    try {
        std::size_t n_ineq = 0, n_eq = 0;
        const auto values = constraint_values(model, x, n_ineq, n_eq);
        if (n_ineq != model.n_ineq || n_eq != model.n_eq ||
            values.size() != model.n_ineq + model.n_eq) {
            return reject("constraint callback output dimension mismatch");
        }
        for (std::size_t i = 0; i < n_ineq; ++i) {
            if (!std::isfinite(values[i])) {
                return reject("inequality callback returned a non-finite value");
            }
            report.maximum_constraint_violation =
                std::max(report.maximum_constraint_violation, std::max(0.0, values[i]));
        }
        for (std::size_t i = n_ineq; i < values.size(); ++i) {
            if (!std::isfinite(values[i])) {
                return reject("equality callback returned a non-finite value");
            }
            report.maximum_constraint_violation =
                std::max(report.maximum_constraint_violation, std::abs(values[i]));
        }
        for (std::size_t j = 0; j < model.n_vars; ++j) {
            const double lb = model.bound_lower(j);
            const double ub = model.bound_upper(j);
            if (std::isfinite(lb)) {
                report.maximum_bound_violation =
                    std::max(report.maximum_bound_violation, std::max(0.0, lb - x[j]));
            }
            if (std::isfinite(ub)) {
                report.maximum_bound_violation =
                    std::max(report.maximum_bound_violation, std::max(0.0, x[j] - ub));
            }
        }
        report.maximum_violation =
            std::max(report.maximum_constraint_violation, report.maximum_bound_violation);
    } catch (const std::exception& e) {
        return reject(std::string("constraint evaluation failed: ") + e.what());
    } catch (...) {
        return reject("constraint evaluation failed with an unknown exception");
    }
    report.feasible = report.maximum_violation <= tolerance;
    report.message = report.feasible ? "primal feasibility verified"
                                     : "primal feasibility violation exceeds tolerance";
    return report;
}

NlpVerificationReport verify_nlp_solution(const NlpModel& model, const SqpSolution& solution,
                                          double tolerance) {
    NlpVerificationReport report;
    const std::vector<double>& x = solution.x;
    const std::size_t n = model.n_vars;

    if (x.size() != n) {
        report.message = "dimension mismatch";
        return report;
    }

    const auto primal = verify_nlp_feasibility(model, x, tolerance);
    if (!primal.feasible) {
        report.inequality_violation = primal.maximum_violation;
        report.message = "primal feasibility check failed: " + primal.message;
        return report;
    }
    try {
        if (!std::isfinite(model.eval_objective(x))) {
            report.message = "objective callback returned a non-finite value";
            return report;
        }
    } catch (const std::exception& e) {
        report.message = std::string("objective callback failed: ") + e.what();
        return report;
    } catch (...) {
        report.message = "objective callback failed with a non-standard exception";
        return report;
    }

    // Feasibility (guarded: contract nlp-local-sqp.md §2.6 — a verification
    // call on raw user callbacks must reject, never throw).
    std::size_t n_ineq = 0, n_eq = 0;
    std::vector<double> cvals;
    try {
        cvals = constraint_values(model, x, n_ineq, n_eq);
    } catch (const std::exception& e) {
        report.message = std::string("constraint callback failed: ") + e.what();
        return report;
    } catch (...) {
        report.message = "constraint callback failed with a non-standard exception";
        return report;
    }
    for (std::size_t i = 0; i < n_ineq; ++i) {
        report.inequality_violation = std::max(report.inequality_violation, cvals[i]);
    }
    for (std::size_t i = n_ineq; i < cvals.size(); ++i) {
        report.equality_violation = std::max(report.equality_violation, std::abs(cvals[i]));
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double lb = model.bound_lower(j);
        const double ub = model.bound_upper(j);
        if (std::isfinite(lb)) {
            report.inequality_violation =
                std::max(report.inequality_violation, lb - x[j]);
        }
        if (std::isfinite(ub)) {
            report.inequality_violation =
                std::max(report.inequality_violation, x[j] - ub);
        }
    }

    // Stationarity: grad f + J^T lambda (uses the solver's multiplier estimate;
    // an independent recomputation of the residual, not of the multipliers).
    std::vector<double> stat;
    std::vector<std::vector<double>> J;
    try {
        stat = model.eval_gradient(x);
        J = constraint_jacobian(model, x, n_ineq, n_eq);
    } catch (const std::exception& e) {
        report.message = std::string("gradient or Jacobian callback failed: ") + e.what();
        return report;
    } catch (...) {
        report.message = "gradient or Jacobian callback failed with a non-standard exception";
        return report;
    }
    if (stat.size() != n || n_ineq != model.n_ineq || n_eq != model.n_eq ||
        J.size() != n_ineq + n_eq ||
        solution.ineq_multipliers.size() != n_ineq ||
        solution.eq_multipliers.size() != n_eq) {
        report.message = "gradient, Jacobian, or multiplier dimensions are inconsistent";
        return report;
    }
    for (const auto& row : J) {
        if (row.size() != n ||
            std::any_of(row.begin(), row.end(), [](double v) { return !std::isfinite(v); })) {
            report.message = "Jacobian callback returned invalid dimensions or non-finite values";
            return report;
        }
    }
    if (std::any_of(stat.begin(), stat.end(), [](double v) { return !std::isfinite(v); }) ||
        std::any_of(solution.ineq_multipliers.begin(), solution.ineq_multipliers.end(),
                    [](double v) { return !std::isfinite(v); }) ||
        std::any_of(solution.eq_multipliers.begin(), solution.eq_multipliers.end(),
                    [](double v) { return !std::isfinite(v); })) {
        report.message = "gradient or multiplier contains a non-finite value";
        return report;
    }
    for (std::size_t i = 0; i < n_ineq && i < solution.ineq_multipliers.size(); ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            stat[j] += solution.ineq_multipliers[i] * J[i][j];
        }
    }
    for (std::size_t i = 0; i < n_eq && i < solution.eq_multipliers.size(); ++i) {
        const auto& row = J[n_ineq + i];
        for (std::size_t j = 0; j < n; ++j) {
            stat[j] += solution.eq_multipliers[i] * row[j];
        }
    }
    // Bound multipliers are implicit in this API. Project stationarity onto
    // the normal cone of the box: lower-active coordinates require r >= 0,
    // upper-active coordinates require r <= 0, and fixed coordinates allow
    // either sign.
    report.stationarity_residual = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        const double lb = model.bound_lower(j);
        const double ub = model.bound_upper(j);
        double residual;
        if (std::isfinite(lb) && std::isfinite(ub) && std::abs(lb - ub) <= 1e-12) {
            residual = 0.0;
        } else if (std::isfinite(lb) && x[j] - lb <= tolerance) {
            residual = std::max(0.0, -stat[j]);
        } else if (std::isfinite(ub) && ub - x[j] <= tolerance) {
            residual = std::max(0.0, stat[j]);
        } else {
            residual = std::abs(stat[j]);
        }
        report.stationarity_residual = std::max(report.stationarity_residual, residual);
    }

    // Dual sign check for the <= 0 convention (lam >= 0).
    for (double lam : solution.ineq_multipliers) {
        report.worst_dual_sign = std::min(report.worst_dual_sign, lam);
    }
    for (std::size_t i = 0; i < n_ineq && i < solution.ineq_multipliers.size(); ++i) {
        report.complementarity_residual = std::max(
            report.complementarity_residual,
            std::abs(solution.ineq_multipliers[i] * cvals[i]));
    }

    report.accepted = report.stationarity_residual <= tolerance &&
                      report.inequality_violation <= tolerance &&
                      report.equality_violation <= tolerance &&
                      report.worst_dual_sign >= -tolerance &&
                      report.complementarity_residual <= tolerance;
    if (!report.message.empty()) {
        // keep
    } else if (report.accepted) {
        report.message = "nlp KKT verified";
    } else {
        report.message = "nlp KKT rejected";
    }
    return report;
}

} // namespace markov_cero::nlp
