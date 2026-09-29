#pragma once

#include "markov_cero/milp/node_view.hpp"
#include "markov_cero/model/model.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace markov_cero::milp {

struct PropagationResult {
    struct Step {
        std::size_t row{}, variable{};
        double coefficient{}, source_rhs{}, derived_bound{};
        bool source_is_lower{};
    };
    bool tightened{false};
    LowerBoundEvidence evidence{};
    std::vector<Step> steps;
};

// Apply only singleton-row implications. The original row is the derivation:
// a*x >= L or a*x <= U, with the inequality reversed for negative a.
// The objective bound is the minimum over the resulting variable box.
inline PropagationResult propagate_singleton_rows(
    const model::Model& model, std::vector<model::Bound>& lower,
    std::vector<model::Bound>& upper, NodeBounds& overlay) {
    PropagationResult result;
    const auto rows = model.matrix.row_count;
    const auto columns = model.matrix.column_count;
    if (lower.size() != columns || upper.size() != columns) return result;
    std::vector<std::size_t> row_column(rows, columns);
    std::vector<double> coefficient(rows, 0.0);
    for (std::size_t column = 0; column < columns; ++column) {
        for (std::size_t entry = model.matrix.column_start[column];
             entry < model.matrix.column_start[column + 1]; ++entry) {
            const auto row = model.matrix.row_index[entry];
            if (row_column[row] == columns) row_column[row] = column;
            if (row_column[row] != column) {
                row_column[row] = columns + 1;
            } else {
                coefficient[row] += model.matrix.value[entry];
            }
        }
    }
    for (std::size_t row = 0; row < rows; ++row) {
        const auto column = row_column[row];
        const double a = coefficient[row];
        if (column >= columns || !std::isfinite(a) || a == 0.0) continue;
        const auto tighten_lower = [&](double value, double source_rhs, bool source_is_lower) {
            if (!std::isfinite(value)) return;
            if (upper[column].is_finite() && value > upper[column].value) return;
            if (!lower[column].is_finite() || value > lower[column].value) {
                lower[column] = model::Bound::finite(value);
                overlay = overlay.with_lower(column, lower[column]);
                result.tightened = true;
                result.steps.push_back({row, column, a, source_rhs, value, source_is_lower});
            }
        };
        const auto tighten_upper = [&](double value, double source_rhs, bool source_is_lower) {
            if (!std::isfinite(value)) return;
            if (lower[column].is_finite() && value < lower[column].value) return;
            if (!upper[column].is_finite() || value < upper[column].value) {
                upper[column] = model::Bound::finite(value);
                overlay = overlay.with_upper(column, upper[column]);
                result.tightened = true;
                result.steps.push_back({row, column, a, source_rhs, value, source_is_lower});
            }
        };
        if (model.row_lower[row].is_finite()) {
            if (a > 0) tighten_lower(model.row_lower[row].value / a,
                                    model.row_lower[row].value, true);
            else tighten_upper(model.row_lower[row].value / a,
                               model.row_lower[row].value, true);
        }
        if (model.row_upper[row].is_finite()) {
            if (a > 0) tighten_upper(model.row_upper[row].value / a,
                                    model.row_upper[row].value, false);
            else tighten_lower(model.row_upper[row].value / a,
                               model.row_upper[row].value, false);
        }
    }
    if (!result.tightened || model.objective_sense != model::ObjectiveSense::minimize ||
        model.has_quadratic_objective || model.has_nlobj_section || model.nlp_callbacks ||
        model.objective.size() != columns) return result;
    double bound = model.objective_offset;
    for (std::size_t column = 0; column < columns; ++column) {
        const double cost = model.objective[column];
        if (cost == 0.0) continue;
        const auto& endpoint = cost > 0 ? lower[column] : upper[column];
        if (!endpoint.is_finite()) return result;
        bound += cost * endpoint.value;
    }
    if (std::isfinite(bound)) result.evidence = LowerBoundEvidence::derived(bound);
    return result;
}

} // namespace markov_cero::milp
