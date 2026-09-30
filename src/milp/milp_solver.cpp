#include "search_context.hpp"
#include "markov_cero/core/solve_context.hpp"
namespace markov_cero::milp::detail {
Result Search::run() {
if (!initialize() || !root_relaxation() || !root_branching()) {
    result.search_ms = std::max(0.0, result.runtime_ms - result.lp_bound_ms - result.incumbent_ms);
    return result;
}
    node_model = root_model;
    // 6. Tree Search Loop
    stop_reason = {};
    // RES-01: the shared context (when wired by the API layer) is polled at
    // this phase boundary and at every node so a cancellation or deadline
    // recorded anywhere in the pipeline reaches the serial search; the
    // engine-local deadline check below still owns the per-node time limit.
    // First stop wins: a root-phase stop above is never overwritten here.
    if (stop_reason.empty() && options.context) {
        const auto shared_reason = options.context->poll();
        if (shared_reason != core::StopReason::none)
            stop_reason = core::to_string(shared_reason);
    }
    while (stop_reason.empty() && !queue.empty() &&
           result.nodes_explored < options.max_nodes) {
        const auto now = std::chrono::steady_clock::now();
        if (options.deadline && now >= *options.deadline) {
            stop_reason = "time limit reached";
            break;
        }
        if (options.context) {
            const auto shared_reason = options.context->poll();
            if (shared_reason != core::StopReason::none) {
                stop_reason = core::to_string(shared_reason);
                break;
            }
        }

        node = queue.top();
        queue.pop();

        // Bound pruning
        if (node->lower_bound >= best_upper_bound - options.absolute_gap_tolerance) {
            continue;
        }

        if (!node_relaxation()) continue;
        if (!separate_cuts()) continue;
        if (!branch()) {
            if (!stop_reason.empty()) break;
            continue;
        }
        // Update global lower bound from active queue. The frontier MINIMUM is
        // required: queue.top() is only that under best_bound ordering (see
        // NodeFrontier).
        if (!queue.empty()) {
            best_lower_bound = std::min(best_upper_bound, queue.min_lower_bound());
        }
        if (!stop_reason.empty()) break;

        // Check relative optimality gap
        if (!best_primal.empty() && best_lower_bound > -std::numeric_limits<double>::infinity()) {
            const double gap = relative_gap(best_upper_bound, best_lower_bound);
            if (gap <= options.relative_gap_tolerance) {
                break;
            }
        }
    }

    // 7. Assemble Final Result
return finish();
}
}
namespace markov_cero::milp {
Result solve(const model::Model& model, const Options& options) { return detail::Search(model, options).run(); }
}
