#include "search_context.hpp"
#include "options_validation.hpp"
namespace markov_cero::milp::detail {
bool Search::initialize() {

    start_time = std::chrono::steady_clock::now();
    options = input_options;
    if (!valid_search_options(options)) {
        result.status = lp::reference::SolveStatus::invalid_options;
        result.message = "invalid MILP tolerance or time limit";
        return false;
    }
    if (!options.deadline && std::isfinite(options.time_limit_seconds) &&
        options.time_limit_seconds > 0.0) {
        options.deadline = start_time + std::chrono::duration_cast<
            std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(options.time_limit_seconds));
    }
    result = {};
    try {
        model.validate();
    } catch (const std::exception& e) {
        result.status = lp::reference::SolveStatus::invalid_model;
        result.message = e.what();
        return false;
    }

    // The B&B implementation's incumbent comparisons and global-bound logic
    // are defined for minimization. Normalize maximization once at the solver
    // boundary, then convert every objective-space result back for callers.
    if (model.objective_sense == model::ObjectiveSense::maximize) {
        auto normalized = model;
        normalized.objective_sense = model::ObjectiveSense::minimize;
        normalized.objective_offset = -normalized.objective_offset;
        for (auto& c : normalized.objective) c = -c;
        for (auto& q : normalized.quadratic_matrix.value) q = -q;
        auto normalized_result = solve(normalized, options);
        if (normalized_result.status == lp::reference::SolveStatus::optimal ||
            !normalized_result.primal.empty())
            normalized_result.objective = -normalized_result.objective;
        if (!std::isnan(normalized_result.best_bound))
            normalized_result.best_bound = -normalized_result.best_bound;
        normalized_result.message = "maximization normalized internally; " +
                                    normalized_result.message;
        result = std::move(normalized_result);
        return false;
    }

    // Keep scorer ownership local to this solve. A process-global hook races
    // when two API clients solve different MILPs concurrently.
    branching_scorer = nullptr;
    result.ml_requested = options.branching_strategy == BranchingStrategy::ml_gnn;
    if (result.ml_requested) {
        result.ml_fallback_reason = "no_ml_branching_node_observed";
    }
#ifdef MARKOV_CERO_ENABLE_ML
    // W2/D-04: install the ONNX scorer when the caller asked for ml_gnn and
    // the model file exists. Any failure here degrades silently to
    // pseudo_cost (LOCKED activation contract: ML is opt-in, never fatal).
    owned_scorer = {};
    if (options.branching_strategy == BranchingStrategy::ml_gnn) {
        try {
            owned_scorer = std::make_unique<ml::OnnxBranchingScorer>(
                options.ml_model_path);
            branching_scorer = owned_scorer.get();
            result.ml_model_loaded = true;
            result.ml_fallback_reason = "no_ml_branching_node_observed";
        } catch (const std::exception& e) {
            // silent fallback: scorer stays null; select_branching_variable
            // takes the pseudo-cost path (and the ml_gnn candidates.size()>200
            // gate would not have passed on small instances anyway).
            result.ml_fallback_reason = std::string("model_load_failed: ") + e.what();
        }
    }
    // W2/D-05: optional strong-branching training log (opened on demand).
    sb_log_stream = {};
    if (!options.strong_branching_log_path.empty()) {
        sb_log_stream.open(options.strong_branching_log_path, std::ios::binary | std::ios::app);
    }
    sb_log_file = sb_log_stream.is_open() ? &sb_log_stream : nullptr;
#else
    if (result.ml_requested) {
        result.ml_fallback_reason = "ml_not_compiled";
    }
#endif

    // Check if model is purely continuous
    bool has_discrete = false;
    for (const auto type : model.variable_type) {
        if (type != model::VariableType::continuous) {
            has_discrete = true;
            break;
        }
    }

    if (!has_discrete) {
        const auto relaxation = solve_node_relaxation(model, options, std::nullopt);
        result.status = relaxation.status;
        result.primal = relaxation.primal;
        result.objective = relaxation.objective;
        result.best_bound = relaxation.lower_bound;
        result.relative_gap = relative_gap(result.objective, result.best_bound);
        result.lp_iterations = relaxation.iterations;
        result.condition_estimate = relaxation.condition_estimate;
        result.nodes_explored = 1;
        result.message = relaxation.message;
        result.runtime_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start_time).count();
        return false;
    }

    next_node_id = 1;
    best_upper_bound = std::numeric_limits<double>::infinity();
    best_lower_bound = -std::numeric_limits<double>::infinity();
    best_primal = {};
    pseudo_costs = std::vector<markov_cero::milp::VariablePseudoCost>(model.matrix.column_count);
    root_model = model;


return true;
}
}
