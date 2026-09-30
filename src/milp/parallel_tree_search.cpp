#include "parallel_tree_search_internal.hpp"
#include "markov_cero/core/worker_context.hpp"
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
                  std::atomic<std::size_t>& total_cuts_generated,
                  std::atomic<std::size_t>& total_heuristics_found,
                  std::atomic<std::size_t>& unresolved_node_lps,
                  std::atomic<double>* worker_bounds,
                  SharedPseudoCosts& shared_pseudo_costs,
                  std::vector<model::Bound>& node_lower,
                  std::vector<model::Bound>& node_upper,
                  NodeBounds::MaterializationScratch& bounds_scratch,
                  const std::function<void()>& clear_bound,
                  ProofEventCollector& proof_events) {
    worker_bounds[thread_id].store(node->lower_bound, std::memory_order_relaxed);
    // MIP-01 contract §1 F1 + §3: only a finite bound may prune, through the guard.
    if (std::isfinite(node->lower_bound) && prune_guard(node->lower_bound) >=
        incumbent.best_incumbent_objective.load(std::memory_order_relaxed) -
            options.absolute_gap_tolerance) {
        clear_bound();
        return;
    }

    node->bounds.materialize(root_model.variable_lower, root_model.variable_upper,
                             node_lower, node_upper, bounds_scratch);
    // Audit-only singleton implications from a private bounds copy (§5).
    proof_events.record_propagations(thread_id, *node, root_model, node_lower, node_upper);

    const auto warm_basis = node->warm_basis
        ? std::optional<lp::dual::BasisState>(*node->warm_basis)
        : std::nullopt;
    const auto explored_count = total_nodes_explored.fetch_add(1, std::memory_order_relaxed) + 1;
    // Cut obligations are recorded inside the with-cuts solve, per round,
    // against each round's pre-cut separation primal (contract §5.2).
    const auto node_lp_res = solve_parallel_node_with_cuts(
        *node, root_model, options, warm_basis, node_lower, node_upper, explored_count,
        total_lp_iterations, total_cuts_generated, proof_events, thread_id);

    if (node_lp_res.status == lp::reference::SolveStatus::infeasible) {
        clear_bound();
        return;
    }
    if (node_lp_res.status != lp::reference::SolveStatus::optimal) {
        // §2 P5: unresolved — hold the bound back; never prunable.
        unresolved_node_lps.fetch_add(1, std::memory_order_relaxed);
        queue.note_dropped_bound(node->lower_bound);
        clear_bound();
        return;
    }
    const double parent_bound = node->lower_bound;
    node->lower_bound = std::max(node->lower_bound, node_lp_res.lower_bound);
    worker_bounds[thread_id].store(node->lower_bound, std::memory_order_relaxed);

    if (std::isfinite(node_lp_res.lower_bound) && prune_guard(node_lp_res.lower_bound) >=
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
    const bool near_integral = fractional_vars.empty();

    if (near_integral) {
        const auto rounded = rounded_integer_candidate(root_model, node_lp_res.primal,
            options.feasibility_tolerance, options.integrality_tolerance);
        if (rounded.found) {
            if (incumbent.update_if_better(rounded.objective, rounded.primal,
                                           options.absolute_gap_tolerance)) {
                queue.prune(incumbent.best_incumbent_objective.load(std::memory_order_relaxed) -
                            options.absolute_gap_tolerance);
            }
            if (std::abs(node_lp_res.objective - rounded.objective) <=
                options.absolute_gap_tolerance) {
                clear_bound();
                return;
            }
        }
        if (find_fractional_variables(node_lp_res.primal,
                root_model.variable_type, 0.0).empty()) {
            unresolved_node_lps.fetch_add(1, std::memory_order_relaxed);
            queue.note_dropped_bound(node->lower_bound);
            clear_bound();
            return;
        }
    }

    if (options.enable_heuristics &&
        total_nodes_explored.load(std::memory_order_relaxed) % 10 == 0) {
        const auto hr = simple_rounding(root_model, node_lower, node_upper, node_lp_res.primal,
                                        options.feasibility_tolerance, options.integrality_tolerance);
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
                                  options.branching_strategy,
                                  near_integral ? 0.0 : options.integrality_tolerance);

    if (branch_var >= root_model.matrix.column_count) {
        // §2 P6: internal failure, not a prune — count and hold bound.
        unresolved_node_lps.fetch_add(1, std::memory_order_relaxed);
        queue.note_dropped_bound(node->lower_bound);
        clear_bound();
        return;
    }

    if (options.context && !options.context->charge_or_stop(2U * sizeof(BranchNode))) {
        // §2 P8: refusal stops the subtree; keep its bound in the frontier.
        queue.note_dropped_bound(node->lower_bound);
        queue.request_stop();
        clear_bound();
        return;
    }
    const auto push_status = push_branch_children(
        queue, *node, branch_var, node_lp_res.primal[branch_var], node->lower_bound,
        node_lower, node_upper, node_lp_res.basis, next_node_id);
    if (push_status == ChildPushStatus::split_rejected) {
        // §2 P6 + §4.2: degenerate split — unresolved, hold the bound.
        unresolved_node_lps.fetch_add(1, std::memory_order_relaxed);
        queue.note_dropped_bound(node->lower_bound);
        clear_bound();
        return;
    }
    if (push_status == ChildPushStatus::empty_integer_domain) {
        clear_bound();  // §4.2: emptiness already recorded by the push
        return;
    }
    if (queue.capacity_exhausted() && options.context)
        (void)options.context->note_stop(core::StopReason::queue_capacity_exhausted);
    clear_bound();
}
}

namespace detail_parallel_tree_search {
void worker_loop(
    std::size_t thread_id, const model::Model& root_model, const ParallelOptions& options,
    ThreadSafeNodeQueue& queue, IncumbentManager& incumbent, std::atomic<std::size_t>& next_node_id,
    std::atomic<std::size_t>& total_nodes_explored, std::atomic<std::size_t>& total_lp_iterations,
    std::atomic<std::size_t>& total_cuts_generated,
    std::atomic<std::size_t>& total_heuristics_found,
    std::atomic<std::size_t>& unresolved_node_lps,
    std::atomic<bool>& interrupted_search, std::atomic<double>* worker_bounds,
    std::size_t num_threads, SharedPseudoCosts& shared_pseudo_costs,
    const std::chrono::steady_clock::time_point start_time, std::stop_token stop_token,
    ProofEventCollector& proof_events) {
    std::optional<core::WorkerContext> worker;
    if (options.context) worker.emplace(*options.context, thread_id);
    bool was_active = false;
    auto node_lower = root_model.variable_lower;
    auto node_upper = root_model.variable_upper;
    NodeBounds::MaterializationScratch bounds_scratch;
    auto clear_bound = [&]() {
        worker_bounds[thread_id].store(std::numeric_limits<double>::infinity(),
                                       std::memory_order_relaxed);
    };

    while (!stop_token.stop_requested() && !queue.is_stopped()) {
        if (worker && worker->poll() != core::StopReason::none) {
            (void)worker->note_local_stop(worker->poll());
            interrupted_search.store(true, std::memory_order_relaxed);
            queue.request_stop();
            break;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto time_spent = std::chrono::duration<double>(now - start_time).count();
        const bool timed_out = time_spent > options.time_limit_seconds || (options.deadline && now >= *options.deadline);
        const bool node_quota = total_nodes_explored.load(std::memory_order_relaxed) >= options.max_nodes;
        if (timed_out || node_quota) {
            const auto reason = timed_out ? core::StopReason::deadline_exceeded
                                          : core::StopReason::quota_exhausted;
            if (worker) (void)worker->note_local_stop(reason);
            if (options.context) (void)options.context->note_stop(reason);
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
                    interrupted_search.store(true, std::memory_order_relaxed);
                    queue.request_stop();
                    break;
                }
            }
        }

        // RW-2: one lock acquisition hands this worker a whole best-bounded
        // batch instead of a per-node mutex round-trip.
        bool became_active = false;
        auto batch = queue.pop_batch(was_active, prune_cutoff, became_active);
        was_active = became_active;

        if (batch.empty()) break;

        // Interleave the batch so concurrent workers explore different regions.
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
            if (worker && worker->poll() != core::StopReason::none) {
                (void)worker->note_local_stop(worker->poll());
                queue.request_stop();
                break;
            }
            if (stop_token.stop_requested() || queue.is_stopped()) {
                break;
            }
            process_node(std::move(node), thread_id, root_model, options, queue, incumbent,
                         next_node_id, total_nodes_explored, total_lp_iterations,
                         total_cuts_generated,
                         total_heuristics_found, unresolved_node_lps,
                         worker_bounds, shared_pseudo_costs,
                         node_lower, node_upper, bounds_scratch, clear_bound, proof_events);
        }
    }

    if (was_active) {
        queue.deactivate_worker();
    }
    clear_bound();
}
}

}
