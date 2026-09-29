#include "parallel_tree_search_internal.hpp"
#include "markov_cero/core/solve_context.hpp"
namespace markov_cero::milp {
using namespace detail_parallel_tree_search;
// Assemble the atomic search counters into the result record; the root node's
// cut count is tracked separately because it is produced before the workers start.
static void fill_search_counters(Result& result, const std::atomic<std::size_t>& nodes,
    const std::atomic<std::size_t>& lp_iterations, const std::atomic<std::size_t>& cuts,
    const std::atomic<std::size_t>& heuristics, std::size_t root_cuts) {
    result.nodes_explored = nodes.load(std::memory_order_relaxed);
    result.lp_iterations = lp_iterations.load(std::memory_order_relaxed);
    result.cuts_generated = root_cuts + cuts.load(std::memory_order_relaxed);
    result.heuristics_found = heuristics.load(std::memory_order_relaxed);
}
Result solve_integer_parallel(const model::Model& model, const ParallelOptions& options,
    std::chrono::steady_clock::time_point start_time) {
    Result result;
    auto elapsed_ms = [&]() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start_time).count(); };
    auto fail_early = [&](lp::reference::SolveStatus st, std::string msg) { result.status=st; result.message=std::move(msg); result.runtime_ms=elapsed_ms(); return result; };
    IncumbentManager incumbent;    std::atomic<std::size_t> next_node_id{1};
    std::atomic<std::size_t> total_nodes_explored{1};
    std::atomic<std::size_t> total_lp_iterations{0};
    std::atomic<std::size_t> total_cuts_generated{0}, total_heuristics_found{0};
    std::size_t root_cuts_generated = 0;
    // Sized by the requested thread count: the thread quota can only shrink
    // the worker team, never grow it, so every worker index stays in range.
    ProofEventCollector proof_events{std::max<std::size_t>(1, options.num_threads)};
    SharedPseudoCosts shared_pseudo_costs;    shared_pseudo_costs.costs.resize(model.matrix.column_count);
    if (options.context &&
        !options.context->charge_or_stop(model.matrix.value.size() *
            (sizeof(double) + sizeof(std::size_t)) +
            model.matrix.column_start.size() * sizeof(std::size_t) + 4096U))
        return fail_early(lp::reference::SolveStatus::resource_limit,
                          "memory budget exhausted at parallel root model");
    model::Model root_model = model;
    const auto root_lp = solve_node_lp(root_model, options, std::nullopt);
    total_lp_iterations.fetch_add(root_lp.iterations, std::memory_order_relaxed);
    result.condition_estimate = root_lp.condition_estimate;
    if (root_lp.status == lp::reference::SolveStatus::infeasible) {
        return fail_early(lp::reference::SolveStatus::infeasible,
                          "root continuous relaxation is infeasible");
    }
    if (root_lp.status != lp::reference::SolveStatus::optimal) {
        return fail_early(root_lp.status, "root continuous relaxation failed: " +
                                              std::to_string(static_cast<int>(root_lp.status)));
    }
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.best_bound = root_lp.lower_bound;
        result.message = "wall-clock deadline reached during parallel root relaxation";
        const auto rounded = rounded_integer_candidate(root_model, root_lp.primal,
            options.feasibility_tolerance, options.integrality_tolerance);
        if (rounded.found) {
            result.primal = rounded.primal;
            result.objective = rounded.objective;
            result.relative_gap = 0.0;
        }
        result.runtime_ms = elapsed_ms();
        proof_events.append_to(result);
        return result;
    }
    double best_lower_bound = root_lp.lower_bound;
    const auto rounded_root = rounded_integer_candidate(root_model, root_lp.primal,
        options.feasibility_tolerance, options.integrality_tolerance);
    if (rounded_root.found &&
        gap_status(rounded_root.objective, root_lp.lower_bound,
                   options.absolute_gap_tolerance) == lp::reference::SolveStatus::optimal) {
        result.status = lp::reference::SolveStatus::optimal;
        result.primal = rounded_root.primal;
        result.objective = rounded_root.objective;
        result.best_bound = root_lp.lower_bound;
        result.relative_gap = 0.0;
        result.nodes_explored = 1;
        result.lp_iterations = total_lp_iterations.load(std::memory_order_relaxed);
        result.message = "root relaxation integer feasible (integer optimal)";
        result.runtime_ms = elapsed_ms();
        proof_events.append_to(result);
        return result;
    }
    if (rounded_root.found)
        (void)incumbent.update_if_better(rounded_root.objective, rounded_root.primal,
                                          options.absolute_gap_tolerance);
    if (options.enable_heuristics) {
        const auto hr = simple_rounding(root_model, root_lp.primal, options.feasibility_tolerance,
                                        options.integrality_tolerance);
        if (hr.found &&
            incumbent.update_if_better(hr.objective, hr.primal, options.absolute_gap_tolerance)) {
            total_heuristics_found.fetch_add(1, std::memory_order_relaxed);
        }
        const auto fp =
            feasibility_pump(root_model, root_lp.primal, options.max_pump_iterations,
                             options.feasibility_tolerance, options.integrality_tolerance);
        if (fp.found &&
            incumbent.update_if_better(fp.objective, fp.primal, options.absolute_gap_tolerance)) {
            total_heuristics_found.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::optional<lp::dual::BasisState> current_basis = root_lp.basis;
    std::vector<double> current_primal = root_lp.primal;
    double current_obj = root_lp.objective;    apply_parallel_root_cuts(root_model, options, root_lp, incumbent,
                             total_lp_iterations, current_primal, current_obj,
                             current_basis, best_lower_bound, root_cuts_generated,
                             proof_events);

    if (incumbent.has_incumbent() && std::isfinite(best_lower_bound)) {
        const double gap = relative_gap(incumbent.get_objective(), best_lower_bound);
        if (gap <= options.relative_gap_tolerance) {
            result.status = gap_status(incumbent.get_objective(), best_lower_bound, options.absolute_gap_tolerance);
            result.primal = incumbent.get_primal();
            result.objective = incumbent.get_objective();
            result.best_bound = best_lower_bound;
            result.relative_gap = gap;
            fill_search_counters(result, total_nodes_explored, total_lp_iterations,
                                 total_cuts_generated, total_heuristics_found, root_cuts_generated);
            result.message = "optimality gap closed at root node";
            result.runtime_ms = elapsed_ms();
            proof_events.append_to(result);
            return result;
        }
    }

    if (options.enable_strong_branching && current_basis.has_value()) {
        try {
            StrongBranchingOptions sb_opts;
            sb_opts.integrality_tolerance = options.integrality_tolerance;
            sb_opts.feasibility_tolerance = options.feasibility_tolerance;
            sb_opts.deadline = options.deadline;
            sb_opts.update_pseudo_costs = true;
            std::vector<VariablePseudoCost> initial_pc = shared_pseudo_costs.costs;
            const auto sb_res = evaluate_strong_branching(root_model, current_primal, current_obj,
                                                          current_basis, sb_opts, &initial_pc);
            if (sb_res.deadline_reached) {
                result.status = lp::reference::SolveStatus::resource_limit;
                result.best_bound = best_lower_bound;
                if (incumbent.has_incumbent()) {
                    result.primal = incumbent.get_primal();
                    result.objective = incumbent.get_objective();
                    result.relative_gap = std::max(0.0, result.objective - best_lower_bound) /
                                          std::max(1.0, std::abs(result.objective));
                }
                result.message = "parallel wall-clock deadline reached during root strong branching";
                result.runtime_ms = elapsed_ms();
                proof_events.append_to(result);
                return result;
            }

            if (sb_res.subproblem_infeasible) {
                return fail_early(lp::reference::SolveStatus::infeasible,
                                  "proven infeasible by strong branching at root");
            }

            {
                std::lock_guard<std::mutex> lock(shared_pseudo_costs.mutex);
                shared_pseudo_costs.costs = std::move(initial_pc);
            }

            for (const auto& dr : sb_res.domain_reductions) {
                if (dr.variable_index < root_model.matrix.column_count) {
                    if (dr.new_lower.is_finite()) {
                        root_model.variable_lower[dr.variable_index] = dr.new_lower;
                    }
                    if (dr.new_upper.is_finite()) {
                        root_model.variable_upper[dr.variable_index] = dr.new_upper;
                    }
                }
            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::length_error&) {
            throw;
        } catch (...) {
        }
    }

    const std::size_t root_branch_var = select_branching_variable(
        current_primal, root_model.variable_type, shared_pseudo_costs.costs,
        options.branching_strategy, options.integrality_tolerance);

    if (root_branch_var >= root_model.matrix.column_count) {
        if (incumbent.has_incumbent()) {
            result.status = lp::reference::SolveStatus::optimal;
            result.primal = incumbent.get_primal();
            result.objective = incumbent.get_objective();
            result.best_bound = incumbent.get_objective();
            result.relative_gap = 0.0;
        } else {
            result.status = lp::reference::SolveStatus::infeasible;
            result.message = "no integer feasible solution found";
        }
        result.runtime_ms = elapsed_ms();
        proof_events.append_to(result);
        return result;
    }

    ThreadSafeNodeQueue queue;
    queue.set_node_selection(options.node_selection);
    queue.set_maximum_size(options.max_queued_nodes);
    BranchNode root_node;
    root_node.id = 0;
    root_node.depth = 0;
    if (options.context && !options.context->charge_or_stop(2U * sizeof(BranchNode)))
        return fail_early(lp::reference::SolveStatus::resource_limit,
                          "memory budget exhausted at parallel root queue");
    (void)queue.push_branch_children(
                               root_node, root_branch_var, current_primal[root_branch_var],
                               best_lower_bound, root_model.variable_lower,
                               root_model.variable_upper,
                               current_basis, next_node_id);
    if (queue.capacity_exhausted() && options.context)
        (void)options.context->note_stop(core::StopReason::queue_capacity_exhausted);

    const std::size_t num_threads = std::max<std::size_t>(1, options.num_threads);
    if (options.context && options.context->thread_quota() != 0 &&
        num_threads > options.context->thread_quota())
        return fail_early(lp::reference::SolveStatus::resource_limit,
                          "parallel worker quota exceeded");
    std::atomic<std::size_t> unresolved_node_lps{0};
    std::atomic<bool> interrupted_search{false};
    std::atomic<bool> worker_failed{false};
    auto worker_bounds = std::make_unique<std::atomic<double>[]>(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
        worker_bounds[i].store(std::numeric_limits<double>::infinity(), std::memory_order_relaxed);
    }

    {
        std::vector<std::jthread> workers;
        workers.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([&, i](std::stop_token st) {
                try {
                    worker_loop(i, root_model, options, queue, incumbent, next_node_id,
                                total_nodes_explored, total_lp_iterations, total_cuts_generated,
                                total_heuristics_found,
                                unresolved_node_lps, interrupted_search, worker_bounds.get(),
                                num_threads, shared_pseudo_costs, start_time, st, proof_events);
                } catch (...) {
                    worker_failed.store(true, std::memory_order_relaxed);
                    queue.request_stop();
                }
            });
        }
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
    if (worker_failed.load(std::memory_order_relaxed))
        return fail_early(lp::reference::SolveStatus::resource_limit,
                          "allocation or exception failure in parallel worker");

    result.runtime_ms = elapsed_ms();
    result.max_queued_nodes = queue.peak_size();
    fill_search_counters(result, total_nodes_explored, total_lp_iterations,
                         total_cuts_generated, total_heuristics_found, root_cuts_generated);
    proof_events.append_to(result);

    classify_parallel_result(result, options, queue, incumbent, worker_bounds.get(),
                             num_threads, best_lower_bound, unresolved_node_lps,
                             interrupted_search);

    return result;
}
}
