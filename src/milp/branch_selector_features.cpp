#include "branch_selector_internal.hpp"
namespace markov_cero::milp {
using namespace detail_branch_selector;
std::vector<NodeFeatureVector>
IBranchingScorer::extract_features(const std::vector<double>& primal,
                                   const std::vector<model::VariableType>& /*types*/,
                                   const std::vector<std::size_t>& candidates,
                                   const std::vector<VariablePseudoCost>& pseudo_costs,
                                   const model::Model& model) const {
    // Note: types is unused here because candidates were already filtered to
    // discrete variables by find_fractional_variables.
    std::vector<NodeFeatureVector> features;
    features.reserve(candidates.size());
    const std::size_t rows = model.matrix.row_count;

    // Global pseudo-cost averages fill in unobserved directions.
    double sum_down = 0.0, sum_up = 0.0;
    std::size_t count_down = 0, count_up = 0;
    for (const auto& pc : pseudo_costs) {
        if (pc.down_count > 0) {
            sum_down += pc.down_cost();
            ++count_down;
        }
        if (pc.up_count > 0) {
            sum_up += pc.up_cost();
            ++count_up;
        }
    }
    const double avg_down = count_down > 0 ? sum_down / count_down : 1.0;
    const double avg_up = count_up > 0 ? sum_up / count_up : 1.0;

    for (std::size_t j : candidates) {
        NodeFeatureVector f;
        f.variable = j;
        const double x = primal[j];
        const double frac = x - std::floor(x);
        f.fractionality = std::min(frac, 1.0 - frac);
        f.objective_coefficient = j < model.objective.size() ? model.objective[j] : 0.0;

        const double down_cost = (j < pseudo_costs.size() && pseudo_costs[j].down_count > 0)
                                     ? pseudo_costs[j].down_cost()
                                     : avg_down;
        const double up_cost = (j < pseudo_costs.size() && pseudo_costs[j].up_count > 0)
                                   ? pseudo_costs[j].up_cost()
                                   : avg_up;
        const double denom = down_cost + up_cost;
        f.pseudocost_down_ratio = denom > 0.0 ? down_cost / denom : 0.5;
        f.pseudocost_up_ratio = denom > 0.0 ? up_cost / denom : 0.5;

        const double lb = j < model.variable_lower.size() && model.variable_lower[j].is_finite()
                              ? model.variable_lower[j].value
                              : 0.0;
        const double ub = j < model.variable_upper.size() && model.variable_upper[j].is_finite()
                              ? model.variable_upper[j].value
                              : lb + 1e4;  // cap unbounded width for feature stability
        f.bound_width = ub - lb;

        // Column density from the CSC pattern.
        if (j < model.matrix.column_count) {
            const std::size_t col_nnz = model.matrix.column_start[j + 1] -
                                        model.matrix.column_start[j];
            f.column_density = rows > 0
                                   ? static_cast<double>(col_nnz) / static_cast<double>(rows)
                                   : 0.0;
        }
        features.push_back(std::move(f));
    }
    return features;
}
BipartiteGraphFeatures extract_bipartite_features(
    const std::vector<double>& primal,
    const std::vector<model::VariableType>& types,
    const std::vector<std::size_t>& candidates,
    const std::vector<VariablePseudoCost>& pseudo_costs,
    const model::Model& model,
    const std::vector<double>& row_duals) {
    class FeatureOnlyScorer final : public IBranchingScorer {
      public:
        std::vector<double> score_candidates(
            const std::vector<NodeFeatureVector>&) const override { return {}; }
    } extractor;

    BipartiteGraphFeatures graph;
    graph.variables = extractor.extract_features(primal, types, candidates,
                                                  pseudo_costs, model);
    const std::size_t row_count = model.matrix.row_count;
    std::vector<std::array<double, 4>> all_row_features(row_count);
    std::vector<double> activity(row_count, 0.0);
    if (primal.size() == model.matrix.column_count) {
        activity = model.matrix.multiply(primal);
    }
    std::vector<std::size_t> row_nnz(row_count, 0);
    for (const std::size_t row : model.matrix.row_index) {
        if (row < row_count) ++row_nnz[row];
    }
    std::vector<bool> active(row_count, false);
    double objective_scale = 1.0;
    for (double c : model.objective) objective_scale = std::max(objective_scale, std::abs(c));
    for (std::size_t i = 0; i < row_count; ++i) {
        const bool has_lower = i < model.row_lower.size() && model.row_lower[i].is_finite();
        const bool has_upper = i < model.row_upper.size() && model.row_upper[i].is_finite();
        const double side = has_upper ? model.row_upper[i].value
                           : has_lower ? model.row_lower[i].value : 0.0;
        const double scale = std::max(1.0, std::abs(side));
        const double normalized_activity = activity[i] / scale;
        const double normalized_side = side / scale;
        double normalized_dual = 0.0;
        if (i < row_duals.size() && std::isfinite(row_duals[i])) {
            normalized_dual = row_duals[i] / objective_scale;
        }
        all_row_features[i] = {normalized_side, normalized_activity, normalized_dual,
                         model.matrix.column_count > 0
                             ? static_cast<double>(row_nnz[i]) /
                                   static_cast<double>(model.matrix.column_count)
                             : 0.0};
        constexpr double kActiveTolerance = 1e-6;
        active[i] = (has_lower && std::abs(activity[i] - model.row_lower[i].value) <=
                                         kActiveTolerance * scale) ||
                    (has_upper && std::abs(activity[i] - model.row_upper[i].value) <=
                                         kActiveTolerance * scale);
    }
    std::vector<std::size_t> row_node(row_count, std::numeric_limits<std::size_t>::max());
    graph.rows.reserve(row_count);
    for (std::size_t i = 0; i < row_count; ++i) {
        if (!active[i]) continue;
        row_node[i] = graph.rows.size();
        graph.rows.push_back(all_row_features[i]);
    }
    for (std::size_t k = 0; k < candidates.size(); ++k) {
        const std::size_t col = candidates[k];
        if (col >= model.matrix.column_count || model.matrix.column_start.size() <= col + 1)
            continue;
        for (std::size_t p = model.matrix.column_start[col];
             p < model.matrix.column_start[col + 1]; ++p) {
            const std::size_t row = model.matrix.row_index[p];
            if (row < row_count && active[row]) {
                graph.edges.push_back({k, row_node[row], model.matrix.value[p]});
            }
        }
    }
    return graph;
}
}
