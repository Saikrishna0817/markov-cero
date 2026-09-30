#include "search_context.hpp"
namespace markov_cero::milp::detail {
Result Search::finish() {
    const auto end_time = std::chrono::steady_clock::now();
    result.runtime_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    result.search_ms = std::max(0.0, result.runtime_ms - result.lp_bound_ms - result.incumbent_ms);
    result.max_queued_nodes = queue.peak_size();
    result.empty_domain_nodes = empty_domain_nodes;

    bool gap_closed = false;
    if (best_lower_bound > -std::numeric_limits<double>::infinity()) {
        const double gap = relative_gap(best_upper_bound, best_lower_bound);
        gap_closed = gap <= options.relative_gap_tolerance;
    }
    // R13/R17: optimality may only be claimed when the tree was exhausted with
    // every node relaxation certified. An uncertified node LP leaves its bound
    // unknown, so the proof is incomplete by construction.
    const bool frontier_complete = queue.empty() && !queue.capacity_exhausted();
    const bool proven = (frontier_complete || gap_closed) && unsolved_node_lps == 0 &&
                        !queue.capacity_exhausted();

    if (!best_primal.empty()) {
        result.primal = std::move(best_primal);
        result.objective = best_upper_bound;
        result.best_bound = (frontier_complete && unsolved_node_lps == 0)
                                ? best_upper_bound
                                : std::min({best_lower_bound, min_unsolved_bound,
                                            queue.min_lower_bound()});
        result.relative_gap = relative_gap(result.objective, result.best_bound);
        if (proven) {
            result.status = gap_status(result.objective, result.best_bound, options.absolute_gap_tolerance);
            result.message = result.status == lp::reference::SolveStatus::optimal
                ? "branch-and-cut tree conclusion within numerical tolerances"
                : "requested relative MIP gap satisfied; exact optimum not established";
        } else {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message =
                unsolved_node_lps > 0
                    ? ("search incomplete: " + std::to_string(unsolved_node_lps) +
                       " node LP(s) could not be certified; optimality not proven")
                    : (stop_reason.empty() ? "search stopped before proof of optimality"
                                           : stop_reason + " before proof of optimality");
        }
    } else if (frontier_complete && unsolved_node_lps == 0) {
        result.status = lp::reference::SolveStatus::infeasible;
        result.message = "no integer feasible solution found";
    } else {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.best_bound = std::min({best_lower_bound, min_unsolved_bound,
                                      queue.min_lower_bound()});
        result.message = unsolved_node_lps > 0
                             ? "search incomplete: node LP could not be certified; infeasibility not proven"
                             : (stop_reason.empty() ? "search stopped with no incumbent"
                                                    : stop_reason + " with no incumbent");
    }
    if (result.ml_requested && result.ml_model_loaded) {
        if (!result.ml_telemetry.fallback_reason.empty()) {
            result.ml_fallback_reason = result.ml_telemetry.fallback_reason;
        } else if (result.ml_telemetry.scored_nodes > 0) {
            result.ml_fallback_reason.clear();
        } else if (result.ml_telemetry.eligible_nodes > 0) {
            result.ml_fallback_reason = "eligible_nodes_not_scored";
        } else if (result.ml_telemetry.maximum_candidate_count > 0) {
            result.ml_fallback_reason = "fractional_candidate_gate_not_met";
        } else {
            result.ml_fallback_reason = "no_fractional_branch_node_observed";
        }
    }
    return result;
}
}
