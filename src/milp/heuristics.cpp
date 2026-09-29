#include "heuristics_internal.hpp"
namespace markov_cero::milp {
using namespace detail_heuristics;
bool check_integer_feasibility(const model::Model& model, const std::vector<double>& primal,
                               double feasibility_tol, double integrality_tol) {
    return check_integer_feasibility(model, model.variable_lower, model.variable_upper, primal,
                                     feasibility_tol, integrality_tol);
}

bool check_integer_feasibility(const model::Model& model,
                               const std::vector<model::Bound>& variable_lower,
                               const std::vector<model::Bound>& variable_upper,
                               const std::vector<double>& primal, double feasibility_tol,
                               double integrality_tol) {
    if (primal.size() != model.matrix.column_count) {
        return false;
    }
    if (variable_lower.size() != primal.size() || variable_upper.size() != primal.size()) {
        return false;
    }

    // 1. Variable bounds and integrality
    for (std::size_t j = 0; j < primal.size(); ++j) {
        const double x = primal[j];
        if (!std::isfinite(x)) {
            return false;
        }
        if (variable_lower[j].is_finite()) {
            if (x < variable_lower[j].value - feasibility_tol) {
                return false;
            }
        }
        if (variable_upper[j].is_finite()) {
            if (x > variable_upper[j].value + feasibility_tol) {
                return false;
            }
        }
        if (model.variable_type[j] != model::VariableType::continuous) {
            const double rounded = std::round(x);
            if (std::abs(x - rounded) > integrality_tol || x != rounded) {
                return false;
            }
        }
    }

    // 2. Row constraint bounds
    const auto ax = model.matrix.multiply(primal);
    for (std::size_t i = 0; i < ax.size(); ++i) {
        if (!std::isfinite(ax[i])) {
            return false;
        }
        if (model.row_lower[i].is_finite()) {
            if (ax[i] < model.row_lower[i].value - feasibility_tol) {
                return false;
            }
        }
        if (model.row_upper[i].is_finite()) {
            if (ax[i] > model.row_upper[i].value + feasibility_tol) {
                return false;
            }
        }
    }

    return true;
}
HeuristicResult rounded_integer_candidate(const model::Model& model,
    const std::vector<double>& primal, double feasibility_tol, double integrality_tol) {
    HeuristicResult result;
    if (primal.size() != model.matrix.column_count) return result;
    result.primal = primal;
    for (std::size_t j = 0; j < primal.size(); ++j) {
        if (model.variable_type[j] == model::VariableType::continuous) continue;
        if (!std::isfinite(primal[j])) return result;
        const double rounded = std::round(primal[j]);
        if (std::abs(primal[j] - rounded) > integrality_tol) return result;
        result.primal[j] = rounded;
    }
    result.found = check_integer_feasibility(model, result.primal, feasibility_tol,
                                              integrality_tol);
    if (result.found) result.objective = compute_objective(model, result.primal);
    return result;
}
double compute_objective(const model::Model& model, const std::vector<double>& primal) {
    long double obj = model.objective_offset;
    const std::size_t n = std::min(primal.size(), model.objective.size());
    for (std::size_t j = 0; j < n; ++j) {
        obj += static_cast<long double>(model.objective[j]) * primal[j];
    }
    if (model.has_quadratic_objective &&
        model.quadratic_matrix.column_count == primal.size()) {
        const auto qx = model.quadratic_matrix.multiply(primal);
        long double q_energy = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            q_energy += static_cast<long double>(primal[j]) * qx[j];
        }
        obj += 0.5 * q_energy;
    }
    return static_cast<double>(obj);
}
HeuristicResult simple_rounding(const model::Model& model,
                                const std::vector<double>& continuous_primal,
                                double feasibility_tol, double integrality_tol) {
    return simple_rounding(model, model.variable_lower, model.variable_upper,
                           continuous_primal, feasibility_tol, integrality_tol);
}

HeuristicResult simple_rounding(const model::Model& model,
                                const std::vector<model::Bound>& variable_lower,
                                const std::vector<model::Bound>& variable_upper,
                                const std::vector<double>& continuous_primal,
                                double feasibility_tol, double integrality_tol) {
    HeuristicResult result;
    if (continuous_primal.size() != model.matrix.column_count ||
        variable_lower.size() != continuous_primal.size() ||
        variable_upper.size() != continuous_primal.size()) {
        return result;
    }

    const auto clamp_var = [&](std::size_t j, double val) {
        if (variable_lower[j].is_finite()) {
            val = std::max(val, variable_lower[j].value);
        }
        if (variable_upper[j].is_finite()) {
            val = std::min(val, variable_upper[j].value);
        }
        return val;
    };

    // Strategy 1: Nearest integer rounding
    std::vector<double> candidate1 = continuous_primal;
    for (std::size_t j = 0; j < candidate1.size(); ++j) {
        if (model.variable_type[j] != model::VariableType::continuous) {
            candidate1[j] = clamp_var(j, std::round(candidate1[j]));
        }
    }
    if (check_integer_feasibility(model, variable_lower, variable_upper, candidate1,
                                  feasibility_tol, integrality_tol)) {
        result.found = true;
        result.primal = candidate1;
        result.objective = compute_objective(model, candidate1);
        return result;
    }

    // Strategy 2: Objective-directed rounding
    std::vector<double> candidate2 = continuous_primal;
    for (std::size_t j = 0; j < candidate2.size(); ++j) {
        if (model.variable_type[j] != model::VariableType::continuous) {
            const double c = model.objective[j];
            const double val = candidate2[j];
            double rounded = std::round(val);
            if (c > 1e-9) {
                rounded = std::floor(val);
            } else if (c < -1e-9) {
                rounded = std::ceil(val);
            }
            candidate2[j] = clamp_var(j, rounded);
        }
    }
    if (check_integer_feasibility(model, variable_lower, variable_upper, candidate2,
                                  feasibility_tol, integrality_tol)) {
        result.found = true;
        result.primal = candidate2;
        result.objective = compute_objective(model, candidate2);
        return result;
    }

    // Strategy 3: Up-rounding (often feasible for covering constraints)
    std::vector<double> candidate3 = continuous_primal;
    for (std::size_t j = 0; j < candidate3.size(); ++j) {
        if (model.variable_type[j] != model::VariableType::continuous) {
            candidate3[j] = clamp_var(j, std::ceil(candidate3[j]));
        }
    }
    if (check_integer_feasibility(model, variable_lower, variable_upper, candidate3,
                                  feasibility_tol, integrality_tol)) {
        result.found = true;
        result.primal = candidate3;
        result.objective = compute_objective(model, candidate3);
        return result;
    }

    return result;
}
}
