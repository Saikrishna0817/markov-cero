#include "markov_cero/milp/cover.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace markov_cero::milp {

std::vector<Cut>
generate_cover_cuts(const model::Model& model,
                    const std::vector<double>& original_primal,
                    std::size_t max_cuts,
                    double min_violation) {
    std::vector<Cut> candidates;
    if (original_primal.size() != model.matrix.column_count) {
        return candidates;
    }

    const auto& mat = model.matrix;
    // Build row-wise view for knapsack inspection
    std::vector<std::vector<std::pair<std::size_t, double>>> row_entries(mat.row_count);
    for (std::size_t col = 0; col < mat.column_count; ++col) {
        const std::size_t start = mat.column_start[col];
        const std::size_t end = (col + 1 < mat.column_start.size()) ? mat.column_start[col + 1] : mat.row_index.size();
        for (std::size_t k = start; k < end; ++k) {
            row_entries[mat.row_index[k]].emplace_back(col, mat.value[k]);
        }
    }

    for (std::size_t i = 0; i < mat.row_count; ++i) {
        if (!model.row_upper[i].is_finite()) {
            continue;
        }
        const double capacity = model.row_upper[i].value;

        // Collect binary variables with positive coefficients
        struct KnapsackItem {
            std::size_t var_idx;
            double coeff;
            double sol_val;
        };
        std::vector<KnapsackItem> items;
        double sum_all = 0.0;

        for (const auto& [col, coeff] : row_entries[i]) {
            if (coeff > 1e-9 && model.variable_type[col] == model::VariableType::binary) {
                items.push_back({col, coeff, original_primal[col]});
                sum_all += coeff;
            }
        }

        if (sum_all <= capacity + 1e-9 || items.size() < 2) {
            continue;
        }

        // Sort items by descending solution value (most violated heuristic)
        std::sort(items.begin(), items.end(), [](const KnapsackItem& a, const KnapsackItem& b) {
            if (std::abs(a.sol_val - b.sol_val) > 1e-6) {
                return a.sol_val > b.sol_val;
            }
            return a.coeff > b.coeff;
        });

        // Find cover C: sum_{j in C} a_j > capacity
        std::vector<KnapsackItem> cover;
        double cover_weight = 0.0;
        for (const auto& item : items) {
            cover.push_back(item);
            cover_weight += item.coeff;
            if (cover_weight > capacity + 1e-9) {
                break;
            }
        }

        if (cover_weight <= capacity + 1e-9) {
            continue;
        }

        // Make cover minimal: remove items if remaining set still forms a cover
        for (auto it = cover.begin(); it != cover.end();) {
            if (cover.size() <= 2) break;
            if (cover_weight - it->coeff > capacity + 1e-9) {
                cover_weight -= it->coeff;
                it = cover.erase(it);
            } else {
                ++it;
            }
        }

        // Compute cut violation: sum_{j in C} x_j - (|C| - 1)
        double lhs_sol = 0.0;
        for (const auto& item : cover) {
            lhs_sol += item.sol_val;
        }
        const double cover_rhs = static_cast<double>(cover.size() - 1);
        const double violation = lhs_sol - cover_rhs;

        if (violation >= min_violation) {
            Cut cut;
            cut.coefficients.assign(model.matrix.column_count, 0.0);
            for (const auto& item : cover) {
                cut.coefficients[item.var_idx] = -1.0; // -x_j
            }
            cut.rhs = -cover_rhs; // sum -x_j >= -( |C| - 1 )  <=>  sum x_j <= |C| - 1
            cut.violation = violation;
            candidates.push_back(cut);
        }
    }

    return filter_cuts(candidates, max_cuts, min_violation);
}

} // namespace markov_cero::milp
