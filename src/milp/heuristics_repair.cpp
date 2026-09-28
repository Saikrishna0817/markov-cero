#include "heuristics_internal.hpp"
namespace markov_cero::milp {
using namespace detail_heuristics;
HeuristicResult local_swap_repair(const model::Model& model,
                                  const std::vector<double>& candidate_primal,
                                  std::size_t max_swaps,
                                  double feasibility_tol,
                                  double integrality_tol) {
    HeuristicResult res;
    if (candidate_primal.size() != model.matrix.column_count) return res;
    std::vector<double> current = candidate_primal;
    if (check_integer_feasibility(model, current, feasibility_tol, integrality_tol)) {
        res.found = true;
        res.primal = current;
        res.objective = compute_objective(model, current);
        return res;
    }

    for (std::size_t swap = 0; swap < max_swaps; ++swap) {
        auto ax = model.matrix.multiply(current);
        double max_viol = 0.0;
        std::size_t worst_row = model.matrix.row_count;
        for (std::size_t i = 0; i < ax.size(); ++i) {
            double v = 0.0;
            if (model.row_lower[i].is_finite() && ax[i] < model.row_lower[i].value - feasibility_tol) {
                v = model.row_lower[i].value - ax[i];
            } else if (model.row_upper[i].is_finite() && ax[i] > model.row_upper[i].value + feasibility_tol) {
                v = ax[i] - model.row_upper[i].value;
            }
            if (v > max_viol) {
                max_viol = v;
                worst_row = i;
            }
        }
        if (worst_row == model.matrix.row_count || max_viol <= feasibility_tol) {
            if (check_integer_feasibility(model, current, feasibility_tol, integrality_tol)) {
                res.found = true;
                res.primal = current;
                res.objective = compute_objective(model, current);
                return res;
            }
            break;
        }

        bool improved = false;
        double best_new_viol = max_viol;
        std::size_t best_j = model.matrix.column_count;
        double best_val = 0.0;

        for (std::size_t j = 0; j < model.matrix.column_count; ++j) {
            if (model.variable_type[j] == model::VariableType::continuous) continue;
            const double lo = model.variable_lower[j].is_finite() ? model.variable_lower[j].value : 0.0;
            const double up = model.variable_upper[j].is_finite() ? model.variable_upper[j].value : 1.0;

            for (double delta : {-1.0, 1.0}) {
                double trial = current[j] + delta;
                if (trial < lo - 1e-6 || trial > up + 1e-6) continue;
                current[j] = trial;
                auto test_ax = model.matrix.multiply(current);
                double test_viol = 0.0;
                for (std::size_t i = 0; i < test_ax.size(); ++i) {
                    if (model.row_lower[i].is_finite() && test_ax[i] < model.row_lower[i].value - feasibility_tol) {
                        test_viol = std::max(test_viol, model.row_lower[i].value - test_ax[i]);
                    } else if (model.row_upper[i].is_finite() && test_ax[i] > model.row_upper[i].value + feasibility_tol) {
                        test_viol = std::max(test_viol, test_ax[i] - model.row_upper[i].value);
                    }
                }
                if (test_viol < best_new_viol) {
                    best_new_viol = test_viol;
                    best_j = j;
                    best_val = trial;
                    improved = true;
                }
                current[j] -= delta;
            }
        }
        if (!improved) break;
        current[best_j] = best_val;
        if (best_new_viol <= feasibility_tol) {
            if (check_integer_feasibility(model, current, feasibility_tol, integrality_tol)) {
                res.found = true;
                res.primal = current;
                res.objective = compute_objective(model, current);
                return res;
            }
        }
    }
    return res;
}
HeuristicResult coefficient_diving(const model::Model& model,
                                   const std::vector<double>& continuous_primal,
                                   std::size_t max_depth,
                                   double feasibility_tol,
                                   double integrality_tol) {
    HeuristicResult result;
    if (continuous_primal.empty()) return result;
    if (check_integer_feasibility(model, continuous_primal, feasibility_tol, integrality_tol)) {
        result.found = true;
        result.primal = continuous_primal;
        result.objective = compute_objective(model, continuous_primal);
        return result;
    }

    auto rounded = simple_rounding(model, continuous_primal, feasibility_tol, integrality_tol);
    if (rounded.found) return rounded;

    std::vector<double> rounded_x = continuous_primal;
    for (std::size_t j = 0; j < model.matrix.column_count; ++j) {
        if (model.variable_type[j] != model::VariableType::continuous) {
            rounded_x[j] = std::round(rounded_x[j]);
        }
    }
    auto repaired = local_swap_repair(model, rounded_x, 20, feasibility_tol, integrality_tol);
    if (repaired.found) return repaired;

    model::Model dive_model = model;
    std::vector<double> current_x = continuous_primal;

    for (std::size_t depth = 0; depth < max_depth; ++depth) {
        std::size_t best_var = model.matrix.column_count;
        double best_dist = 1.0;
        double best_fix = 0.0;

        for (std::size_t j = 0; j < model.matrix.column_count; ++j) {
            if (dive_model.variable_type[j] == model::VariableType::continuous) continue;
            const double val = current_x[j];
            const double round_val = std::round(val);
            const double dist = std::abs(val - round_val);
            if (dist > integrality_tol && dist < best_dist) {
                best_dist = dist;
                best_var = j;
                best_fix = round_val;
            }
        }
        if (best_var == model.matrix.column_count) {
            if (check_integer_feasibility(dive_model, current_x, feasibility_tol, integrality_tol)) {
                result.found = true;
                result.primal = current_x;
                result.objective = compute_objective(model, current_x);
                return result;
            }
            break;
        }

        dive_model.variable_lower[best_var] = model::Bound::finite(best_fix);
        dive_model.variable_upper[best_var] = model::Bound::finite(best_fix);

        try {
            const auto canon = transform::sparse_canonicalize(dive_model, true);
            const auto dense = canon.to_dense();
            lp::reference::Options opts;
            opts.iteration_limit = 2000;
            const auto lpres = lp::reference::solve(dense, opts);
            if (lpres.status != lp::reference::SolveStatus::optimal) break;
            current_x = transform::reconstruct_primal(canon, lpres.primal);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            break;
        }

        if (check_integer_feasibility(model, current_x, feasibility_tol, integrality_tol)) {
            result.found = true;
            result.primal = current_x;
            result.objective = compute_objective(model, current_x);
            return result;
        }
    }
    return result;
}
}
