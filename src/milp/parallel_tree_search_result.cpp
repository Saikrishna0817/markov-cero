// Result classification for the parallel tree search: turns the final tree
// state into an honest status, incumbent, global bound and gap combination.
// Kept separate from tree orchestration so proof state and status semantics
// can be audited without reading the search machinery.
#include "parallel_tree_search_internal.hpp"
#include "markov_cero/core/solve_context.hpp"

namespace markov_cero::milp {
using namespace detail_parallel_tree_search;

void classify_parallel_result(
    Result& result, const ParallelOptions& options, ThreadSafeNodeQueue& queue,
    IncumbentManager& incumbent, const std::atomic<double>* worker_bounds,
    std::size_t num_threads, double best_lower_bound,
    const std::atomic<std::size_t>& unresolved_node_lps,
    const std::atomic<bool>& interrupted_search) {
    const bool deadline_reached = options.deadline &&
                                  std::chrono::steady_clock::now() >= *options.deadline;
    const bool frontier_exhausted = queue.empty() && queue.active_workers() == 0;
    const bool search_complete = frontier_exhausted && !queue.capacity_exhausted() &&
                                 unresolved_node_lps.load(std::memory_order_relaxed) == 0 &&
                                 !interrupted_search.load(std::memory_order_relaxed) &&
                                 !(options.context && options.context->stopped()) &&
                                 !deadline_reached;
    if (incumbent.has_incumbent()) {
        result.primal = incumbent.get_primal();
        result.objective = incumbent.get_objective();

        const double final_lb =
            compute_tree_lower_bound(queue, worker_bounds, num_threads, result.objective);
        // The root relaxation remains a valid bound even if a stopped worker
        // held an unprocessed batch that is no longer in the shared queue.
        result.best_bound = search_complete
                                ? result.objective
                                : (std::isfinite(final_lb)
                                       ? std::min(best_lower_bound, final_lb)
                                       : best_lower_bound);
        result.relative_gap = relative_gap(result.objective, result.best_bound);
        if (deadline_reached) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "parallel MILP wall-clock deadline reached with incumbent";
        } else if (queue.capacity_exhausted()) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "parallel queued-node capacity reached; optimality not proven";
        } else if (search_complete &&
                   result.relative_gap <= options.relative_gap_tolerance) {
            result.status = lp::reference::SolveStatus::optimal;
            result.message = "parallel tree search MILP optimum";
        } else {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = unresolved_node_lps.load(std::memory_order_relaxed) > 0
                                 ? "parallel MILP node LP unresolved; optimality not proven"
                                 : "parallel MILP stopped before proof of optimality";
        }
    } else {
        const double incomplete_tree_bound = compute_tree_lower_bound(
            queue, worker_bounds, num_threads,
            std::numeric_limits<double>::infinity());
        if (std::isfinite(incomplete_tree_bound))
            result.best_bound = std::min(best_lower_bound, incomplete_tree_bound);
        if (deadline_reached) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "parallel MILP wall-clock deadline reached without incumbent";
        } else if (queue.capacity_exhausted()) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "parallel queued-node capacity reached; infeasibility not proven";
        } else if (search_complete) {
            result.status = lp::reference::SolveStatus::infeasible;
            result.message = "no integer feasible solution found";
        } else {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = unresolved_node_lps.load(std::memory_order_relaxed) > 0
                                 ? "parallel MILP node LP unresolved; infeasibility not proven"
                                 : "parallel MILP stopped before finding an incumbent";
        }
    }
}

}
