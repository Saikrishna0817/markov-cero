#include "parallel_tree_search_internal.hpp"
namespace markov_cero::milp {
using namespace detail_parallel_tree_search;
namespace detail_parallel_tree_search {
NodeLpResult solve_node_lp(const model::Model& model, const ParallelOptions& options,
                           const std::optional<lp::dual::BasisState>& warm_start) {
    return solve_node_lp(model, options, warm_start, model.variable_lower,
                         model.variable_upper);
}

NodeLpResult solve_node_lp(const model::Model& model, const ParallelOptions& options,
                           const std::optional<lp::dual::BasisState>& warm_start,
                           const std::vector<model::Bound>& variable_lower,
                           const std::vector<model::Bound>& variable_upper) {
    Options serial;
    serial.max_iterations = options.max_iterations;
    serial.feasibility_tolerance = options.feasibility_tolerance;
    serial.enable_warm_start = options.enable_warm_start;
    serial.deadline = options.deadline;
    return solve_node_relaxation(model, serial, warm_start, variable_lower, variable_upper);
}
}

namespace detail_parallel_tree_search {
void process_node(std::shared_ptr<BranchNode>&& node, std::size_t thread_id,
                  const model::Model& root_model, const ParallelOptions& options,
                  ThreadSafeNodeQueue& queue, IncumbentManager& incumbent,
                  std::atomic<std::size_t>& next_node_id,
                  std::atomic<std::size_t>& total_nodes_explored,
                  std::atomic<std::size_t>& total_lp_iterations,
                  std::atomic<std::size_t>& total_heuristics_found,
                  std::atomic<std::size_t>& unresolved_node_lps,
                  std::atomic<double>* worker_bounds,
                  SharedPseudoCosts& shared_pseudo_costs,
                  std::vector<model::Bound>& node_lower,
                  std::vector<model::Bound>& node_upper,
                  NodeBounds::MaterializationScratch& bounds_scratch,
                  const std::function<void()>& clear_bound) {
    worker_bounds[thread_id].store(node->lower_bound, std::memory_order_relaxed);

    if (node->lower_bound >=
        incumbent.best_incumbent_objective.load(std::memory_order_relaxed) -
            options.absolute_gap_tolerance) {
        clear_bound();
        return;
    }

    node->bounds.materialize(root_model.variable_lower, root_model.variable_upper,
                             node_lower, node_upper, bounds_scratch);

    const auto warm_basis = node->warm_basis
        ? std::optional<lp::dual::BasisState>(*node->warm_basis)
        : std::nullopt;
    const auto node_lp_res = solve_node_lp(root_model, options, warm_basis,
                                           node_lower, node_upper);
    total_lp_iterations.fetch_add(node_lp_res.iterations, std::memory_order_relaxed);
    total_nodes_explored.fetch_add(1, std::memory_order_relaxed);

    if (node_lp_res.status == lp::reference::SolveStatus::infeasible) {
        clear_bound();
        return;
    }
    if (node_lp_res.status != lp::reference::SolveStatus::optimal) {
        unresolved_node_lps.fetch_add(1, std::memory_order_relaxed);
        clear_bound();
        return;
    }

    const double parent_bound = node->lower_bound;
    node->lower_bound = std::max(node->lower_bound, node_lp_res.lower_bound);
    worker_bounds[thread_id].store(node->lower_bound, std::memory_order_relaxed);

    if (node_lp_res.lower_bound >=
        incumbent.best_incumbent_objective.load(std::memory_order_relaxed) -
            options.absolute_gap_tolerance) {
        clear_bound();
        return;
    }

    if (node->depth > 0) {
        const std::size_t b_var = node->branch_variable;
        const double delta_z = node->lower_bound - parent_bound;
        const double frac = node->branch_value - std::floor(node->branch_value);
        std::lock_guard<std::mutex> pc_lock(shared_pseudo_costs.mutex);
        if (node->is_down_branch) {
            shared_pseudo_costs.costs[b_var].record_down(delta_z, frac);
        } else {
            shared_pseudo_costs.costs[b_var].record_up(delta_z, 1.0 - frac);
        }
    }

    const auto fractional_vars = find_fractional_variables(
        node_lp_res.primal, root_model.variable_type, options.integrality_tolerance);

    if (fractional_vars.empty()) {
        if (incumbent.update_if_better(node_lp_res.objective, node_lp_res.primal,
                                       options.absolute_gap_tolerance)) {
            queue.prune(incumbent.best_incumbent_objective.load(std::memory_order_relaxed) -
                        options.absolute_gap_tolerance);
        }
        clear_bound();
        return;
    }

    if (options.enable_heuristics &&
        total_nodes_explored.load(std::memory_order_relaxed) % 10 == 0) {
        const auto hr = simple_rounding(root_model, node_lower, node_upper,
                                        node_lp_res.primal, options.feasibility_tolerance,
                                        options.integrality_tolerance);
        if (hr.found && incumbent.update_if_better(hr.objective, hr.primal,
                                                   options.absolute_gap_tolerance)) {
            total_heuristics_found.fetch_add(1, std::memory_order_relaxed);
            queue.prune(incumbent.best_incumbent_objective.load(std::memory_order_relaxed) -
                        options.absolute_gap_tolerance);
        }
    }

    std::vector<VariablePseudoCost> pc_snapshot;
    {
        std::lock_guard<std::mutex> pc_lock(shared_pseudo_costs.mutex);
        pc_snapshot = shared_pseudo_costs.costs;
    }
    const std::size_t branch_var =
        select_branching_variable(node_lp_res.primal, root_model.variable_type, pc_snapshot,
                                  options.branching_strategy, options.integrality_tolerance);

    if (branch_var >= root_model.matrix.column_count) {
        clear_bound();
        return;
    }

    (void)queue.push_branch_children(*node, branch_var, node_lp_res.primal[branch_var],
                               node->lower_bound, node_lower, node_upper,
                               node_lp_res.basis, next_node_id);
    clear_bound();
}
}

namespace detail_parallel_tree_search {
void worker_loop(
    std::size_t thread_id, const model::Model& root_model, const ParallelOptions& options,
    ThreadSafeNodeQueue& queue, IncumbentManager& incumbent, std::atomic<std::size_t>& next_node_id,
    std::atomic<std::size_t>& total_nodes_explored, std::atomic<std::size_t>& total_lp_iterations,
    std::atomic<std::size_t>& total_heuristics_found,
    std::atomic<std::size_t>& unresolved_node_lps,
    std::atomic<bool>& interrupted_search, std::atomic<double>* worker_bounds,
    std::size_t num_threads, SharedPseudoCosts& shared_pseudo_costs,
    const std::chrono::steady_clock::time_point start_time, std::stop_token stop_token) {


    bool was_active = false;
    auto node_lower = root_model.variable_lower;
    auto node_upper = root_model.variable_upper;
    NodeBounds::MaterializationScratch bounds_scratch;
    auto clear_bound = [&]() {
        worker_bounds[thread_id].store(std::numeric_limits<double>::infinity(),
                                       std::memory_order_relaxed);
    };

    while (!stop_token.stop_requested() && !queue.is_stopped()) {
        const auto now = std::chrono::steady_clock::now();
        const auto time_spent = std::chrono::duration<double>(now - start_time).count();
        if (time_spent > options.time_limit_seconds ||
            (options.deadline && now >= *options.deadline) ||
            total_nodes_explored.load(std::memory_order_relaxed) >= options.max_nodes) {
            ;
            interrupted_search.store(true, std::memory_order_relaxed);
            queue.request_stop();
            break;
        }

        const double current_incumbent =
            incumbent.best_incumbent_objective.load(std::memory_order_relaxed);
        const double prune_cutoff = current_incumbent - options.absolute_gap_tolerance;

        if (incumbent.has_incumbent()) {
            const double current_inc = incumbent.get_objective();
            const double tree_lb =
                compute_tree_lower_bound(queue, worker_bounds, num_threads, current_inc);
            if (tree_lb > -1e15) {
                const double gap =
                    std::abs(current_inc - tree_lb) / std::max(1.0, std::abs(current_inc));
                if (gap <= options.relative_gap_tolerance) {
                    ;
                    interrupted_search.store(true, std::memory_order_relaxed);
                    queue.request_stop();
                    break;
                }
            }
        }

        // RW-2: one lock acquisition hands this worker a whole best-bounded batch
        // (interleaved order: best, worst, 2nd-best, ...). Processing the batch
        // locally removes the per-node mutex round-trip and the per-pop O(n)
        // heap prune that serialized all workers in the old design.
        bool became_active = false;
        auto batch = queue.pop_batch(was_active, prune_cutoff, became_active);
        was_active = became_active;

        if (batch.empty()) {
            ;
            break;
        }

        // Interleave the batch so concurrent workers explore different subtree
        // regions first (soft work stealing): b0, b_{n-1}, b1, b_{n-2}, ...
        if (batch.size() > 2) {
            std::vector<std::shared_ptr<BranchNode>> interleaved;
            interleaved.reserve(batch.size());
            std::size_t lo = 0, hi = batch.size() - 1;
            bool take_low = true;
            while (lo <= hi) {
                interleaved.push_back(batch[take_low ? lo++ : hi--]);
                take_low = !take_low;
            }
            batch = std::move(interleaved);
        }

        for (auto& node : batch) {
            if (stop_token.stop_requested() || queue.is_stopped()) {
                break;
            }
            process_node(std::move(node), thread_id, root_model, options, queue, incumbent,
                         next_node_id, total_nodes_explored, total_lp_iterations,
                         total_heuristics_found, unresolved_node_lps,
                         worker_bounds, shared_pseudo_costs,
                         node_lower, node_upper, bounds_scratch, clear_bound);
        }
    }

    if (was_active) {
        queue.deactivate_worker();
    }
    clear_bound();
}
}

}
