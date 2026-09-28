#include "markov_cero/milp/branch_selector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace markov_cero::milp {

double VariablePseudoCost::down_cost() const noexcept {
    if (down_count > 0) {
        return down_sum / static_cast<double>(down_count);
    }
    return 1.0;
}

double VariablePseudoCost::up_cost() const noexcept {
    if (up_count > 0) {
        return up_sum / static_cast<double>(up_count);
    }
    return 1.0;
}

void VariablePseudoCost::record_down(double delta_obj, double fraction) noexcept {
    if (fraction > 1e-6 && std::isfinite(delta_obj) && delta_obj >= 0.0) {
        down_sum += delta_obj / fraction;
        ++down_count;
    }
}

void VariablePseudoCost::record_up(double delta_obj, double fraction) noexcept {
    if (fraction > 1e-6 && std::isfinite(delta_obj) && delta_obj >= 0.0) {
        up_sum += delta_obj / fraction;
        ++up_count;
    }
}

std::vector<std::size_t> find_fractional_variables(const std::vector<double>& primal,
                                                   const std::vector<model::VariableType>& types,
                                                   double integrality_tol) {
    std::vector<std::size_t> candidates;
    const std::size_t n = std::min(primal.size(), types.size());
    for (std::size_t j = 0; j < n; ++j) {
        if (types[j] == model::VariableType::continuous) {
            continue;
        }
        const double x = primal[j];
        if (!std::isfinite(x)) {
            continue;
        }
        const double rounded = std::round(x);
        if (std::abs(x - rounded) > integrality_tol) {
            candidates.push_back(j);
        }
    }
    return candidates;
}

std::size_t select_most_fractional(const std::vector<double>& primal,
                                   const std::vector<std::size_t>& candidates) {
    if (candidates.empty()) {
        throw std::invalid_argument("candidates list is empty in select_most_fractional");
    }

    std::size_t best_var = candidates[0];
    double max_fractionality = -1.0;

    for (std::size_t j : candidates) {
        const double x = primal[j];
        const double frac = x - std::floor(x);
        const double distance = std::min(frac, 1.0 - frac);
        if (distance > max_fractionality) {
            max_fractionality = distance;
            best_var = j;
        }
    }
    return best_var;
}

std::size_t select_pseudo_cost(const std::vector<double>& primal,
                               const std::vector<std::size_t>& candidates,
                               const std::vector<VariablePseudoCost>& pseudo_costs) {
    if (candidates.empty()) {
        throw std::invalid_argument("candidates list is empty in select_pseudo_cost");
    }

    // Compute global average pseudo costs across observed variables
    double avg_down = 1.0;
    double avg_up = 1.0;
    double sum_down = 0.0;
    double sum_up = 0.0;
    std::size_t count_down = 0;
    std::size_t count_up = 0;

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
    if (count_down > 0) {
        avg_down = sum_down / static_cast<double>(count_down);
    }
    if (count_up > 0) {
        avg_up = sum_up / static_cast<double>(count_up);
    }

    std::size_t best_var = candidates[0];
    double max_score = -1.0;

    for (std::size_t j : candidates) {
        const double x = primal[j];
        const double frac = x - std::floor(x);
        const double down_frac = frac;
        const double up_frac = 1.0 - frac;

        const double down_cost = (j < pseudo_costs.size() && pseudo_costs[j].down_count > 0)
                                     ? pseudo_costs[j].down_cost()
                                     : avg_down;
        const double up_cost = (j < pseudo_costs.size() && pseudo_costs[j].up_count > 0)
                                   ? pseudo_costs[j].up_cost()
                                   : avg_up;

        const double down_deg = down_cost * down_frac;
        const double up_deg = up_cost * up_frac;

        // Standard Achterberg product score: (delta_down + eps) * (delta_up + eps)
        constexpr double eps = 1e-6;
        const double score = (down_deg + eps) * (up_deg + eps);

        if (score > max_score) {
            max_score = score;
            best_var = j;
        }
    }
    return best_var;
}

std::size_t select_branching_variable(const std::vector<double>& primal,
                                      const std::vector<model::VariableType>& types,
                                      const std::vector<VariablePseudoCost>& pseudo_costs,
                                      BranchingStrategy strategy, double integrality_tol,
                                      const model::Model* feature_model,
                                      const IBranchingScorer* scorer,
                                      MlBranchingTelemetry* telemetry,
                                      const std::vector<double>* row_duals) {
    const auto candidates = find_fractional_variables(primal, types, integrality_tol);
    if (candidates.empty()) {
        return types.size(); // None fractional
    }
    if (strategy == BranchingStrategy::most_fractional) {
        return select_most_fractional(primal, candidates);
    }
    // W2/D-04 locked activation gate: use ML only when this node has more
    // than 200 fractional integer candidates. Otherwise use pseudo-cost.
    if (strategy == BranchingStrategy::ml_gnn) {
        if (telemetry != nullptr) {
            telemetry->maximum_candidate_count =
                std::max(telemetry->maximum_candidate_count, candidates.size());
            if (candidates.size() > 200) {
                ++telemetry->eligible_nodes;
            }
        }
        const auto record_fallback = [telemetry](const std::string& reason) {
            if (telemetry == nullptr) return;
            ++telemetry->fallback_nodes;
            if (telemetry->fallback_reason.empty()) {
                telemetry->fallback_reason = reason;
            }
        };
        if (scorer == nullptr) {
            record_fallback("no_ml_scorer_loaded");
        } else if (candidates.size() <= 200) {
            record_fallback("fractional_candidate_gate_not_met");
        } else {
            try {
                // The current node model and solver-owned scorer are passed
                // explicitly; no process-global scorer state can leak across
                // concurrent API calls.
                const model::Model empty_model{};
                const model::Model& active_model = feature_model != nullptr
                                                       ? *feature_model
                                                       : empty_model;
                const auto graph = extract_bipartite_features(
                    primal, types, candidates, pseudo_costs, active_model,
                    row_duals != nullptr ? *row_duals : std::vector<double>{});
                const auto scores = scorer->score_graph(graph);
                if (scores.size() == candidates.size() &&
                    std::all_of(scores.begin(), scores.end(),
                                [](double score) { return std::isfinite(score); })) {
                    if (telemetry != nullptr) {
                        ++telemetry->scored_nodes;
                        telemetry->candidates_scored += candidates.size();
                    }
                    std::size_t best = 0;
                    for (std::size_t k = 1; k < scores.size(); ++k) {
                        if (scores[k] > scores[best]) {
                            best = k;
                        }
                    }
                    return candidates[best];
                }
                record_fallback("invalid_ml_score_vector");
            } catch (const std::exception& e) {
                record_fallback(std::string("ml_scoring_failed: ") + e.what());
            } catch (...) {
                record_fallback("ml_scoring_failed: unknown exception");
            }
        }
        return select_pseudo_cost(primal, candidates, pseudo_costs);
    }
    // pseudo_cost, strong_branching, and reliability fallback to pseudo-cost
    // when called without an active solver/basis state context
    return select_pseudo_cost(primal, candidates, pseudo_costs);
}

// W2: default feature extraction shared by all scorers. Uses only the model
// structure and pseudo-cost state; no LP re-solves.
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

} // namespace markov_cero::milp
