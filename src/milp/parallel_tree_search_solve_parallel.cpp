#include "parallel_tree_search_internal.hpp"
#include "options_validation.hpp"
namespace markov_cero::milp {
using namespace detail_parallel_tree_search;
Result solve_parallel(const model::Model& model, const ParallelOptions& input_options) {
    const auto start_time = std::chrono::steady_clock::now();
    ParallelOptions options = input_options;
    if (!detail::valid_search_options(options) || options.num_threads == 0 || options.num_threads > 256) {
        Result invalid; invalid.status = lp::reference::SolveStatus::invalid_options;
        invalid.message = "invalid parallel tolerance, time limit or worker count (1..256)";
        return invalid;
    }
    if (!options.deadline && std::isfinite(options.time_limit_seconds) &&
        options.time_limit_seconds > 0.0) {
        options.deadline = start_time + std::chrono::duration_cast<
            std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(options.time_limit_seconds));
    }
    Result result;

    auto elapsed_ms = [&]() {
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_time).count();
    };

    auto fail_early = [&](lp::reference::SolveStatus st, std::string msg) {
        result.status = st;
        result.message = std::move(msg);
        result.runtime_ms = elapsed_ms();
        return result;
    };

    try {
        model.validate();
    } catch (const std::exception& e) {
        return fail_early(lp::reference::SolveStatus::invalid_model, e.what());
    }

    if (model.objective_sense == model::ObjectiveSense::maximize) {
        auto normalized = model;
        normalized.objective_sense = model::ObjectiveSense::minimize;
        normalized.objective_offset = -normalized.objective_offset;
        for (auto& c : normalized.objective) c = -c;
        for (auto& q : normalized.quadratic_matrix.value) q = -q;
        auto output = solve_parallel(normalized, options);
        if (!output.primal.empty()) output.objective = -output.objective;
        if (!std::isnan(output.best_bound)) output.best_bound = -output.best_bound;
        return output;
    }
    bool has_discrete = false;
    for (const auto type : model.variable_type) {
        if (type != model::VariableType::continuous) {
            has_discrete = true;
            break;
        }
    }

    if (!has_discrete) {
        const auto relaxation = solve_node_lp(model, options, std::nullopt);
        result.status = relaxation.status;
        result.primal = relaxation.primal;
        result.objective = relaxation.objective;
        result.best_bound = relaxation.lower_bound;
        result.relative_gap = relative_gap(result.objective, result.best_bound);
        result.lp_iterations = relaxation.iterations;
        result.condition_estimate = relaxation.condition_estimate;
        result.nodes_explored = 1;
        result.message = relaxation.message;
        result.runtime_ms = elapsed_ms();
        return result;
    }

    return solve_integer_parallel(model, options, start_time);
}
}
