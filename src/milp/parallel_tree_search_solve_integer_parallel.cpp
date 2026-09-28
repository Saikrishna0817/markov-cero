#include "parallel_tree_search_internal.hpp"
namespace markov_cero::milp {
using namespace detail_parallel_tree_search;
Result solve_integer_parallel(const model::Model& model, const ParallelOptions& options,
    std::chrono::steady_clock::time_point start_time) {
    Result result;
    auto elapsed_ms = [&]() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start_time).count(); };
    auto fail_early = [&](lp::reference::SolveStatus st, std::string msg) { result.status=st; result.message=std::move(msg); result.runtime_ms=elapsed_ms(); return result; };
    IncumbentManager incumbent;    std::atomic<std::size_t> next_node_id{1};
    std::atomic<std::size_t> total_nodes_explored{1};
    std::atomic<std::size_t> total_lp_iterations{0};
    std::atomic<std::size_t> total_heuristics_found{0};
    std::size_t root_cuts_generated = 0;

    SharedPseudoCosts shared_pseudo_costs;    shared_pseudo_costs.costs.resize(model.matrix.column_count);

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
        if (check_integer_feasibility(root_model, root_lp.primal,
                                      options.feasibility_tolerance,
                                      options.integrality_tolerance)) {
            result.primal = root_lp.primal;
            result.objective = root_lp.objective;
            result.relative_gap = 0.0;
        }
        result.runtime_ms = elapsed_ms();
        return result;
    }

    double best_lower_bound = root_lp.lower_bound;

    if (check_integer_feasibility(root_model, root_lp.primal, options.feasibility_tolerance,
                                  options.integrality_tolerance)) {
        result.status = lp::reference::SolveStatus::optimal;
        result.primal = root_lp.primal;
        result.objective = root_lp.objective;
        result.best_bound = root_lp.lower_bound;
        result.relative_gap = 0.0;
        result.nodes_explored = 1;
        result.lp_iterations = total_lp_iterations.load(std::memory_order_relaxed);
        result.message = "root relaxation integer feasible (integer optimal)";
        result.runtime_ms = elapsed_ms();
        return result;
    }

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
    double current_obj = root_lp.objective;

    if (options.enable_cuts && root_lp.basis.has_value()) {
        try {
            const auto canon =
                transform::sparse_canonicalize(root_model, /*relax_integrality=*/true);
            std::vector<Cut> cuts = generate_gomory_cuts(root_model, current_primal, canon,
                                                         *root_lp.basis, options.max_cut_rounds);
            if (options.enable_mir_cuts) {
                const auto mir_cuts = generate_mir_cuts(root_model, current_primal, canon,
                                                        *root_lp.basis, options.max_cut_rounds);
                cuts.insert(cuts.end(), mir_cuts.begin(), mir_cuts.end());
            }
            cuts = filter_cuts(std::move(cuts), options.max_cut_rounds);
            if (!cuts.empty()) {
                add_cuts_to_model(root_model, cuts);
                root_cuts_generated = cuts.size();

                const auto cut_lp = solve_node_lp(root_model, options, root_lp.basis);
                total_lp_iterations.fetch_add(cut_lp.iterations, std::memory_order_relaxed);
                if (cut_lp.status == lp::reference::SolveStatus::optimal) {
                    current_primal = cut_lp.primal;
                    current_obj = cut_lp.objective;
                    current_basis = cut_lp.basis;
                    best_lower_bound = std::max(best_lower_bound, cut_lp.lower_bound);

                    if (check_integer_feasibility(root_model, current_primal,
                                                  options.feasibility_tolerance,
                                                  options.integrality_tolerance)) {
                        incumbent.update_if_better(current_obj, current_primal,
                                                   options.absolute_gap_tolerance);
                    }
                }
            }
        } catch (...) {
        }
    }

    if (incumbent.has_incumbent() && std::isfinite(best_lower_bound)) {
        const double gap = relative_gap(incumbent.get_objective(), best_lower_bound);
        if (gap <= options.relative_gap_tolerance) {
            result.status = gap_status(incumbent.get_objective(), best_lower_bound, options.absolute_gap_tolerance);
            result.primal = incumbent.get_primal();
            result.objective = incumbent.get_objective();
            result.best_bound = best_lower_bound;
            result.relative_gap = gap;
            result.nodes_explored = 1;
            result.lp_iterations = total_lp_iterations.load(std::memory_order_relaxed);
            result.cuts_generated = root_cuts_generated;
            result.heuristics_found = total_heuristics_found.load(std::memory_order_relaxed);
            result.message = "optimality gap closed at root node";
            result.runtime_ms = elapsed_ms();
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
        return result;
    }

    ThreadSafeNodeQueue queue;
    queue.set_node_selection(options.node_selection);
    BranchNode root_node;
    root_node.id = 0;
    root_node.depth = 0;
    root_node.variable_lower = root_model.variable_lower;
    root_node.variable_upper = root_model.variable_upper;

    queue.push_branch_children(root_node, root_branch_var, current_primal[root_branch_var],
                               current_obj, current_basis, next_node_id);

    const std::size_t num_threads = std::max<std::size_t>(1, options.num_threads);
    std::atomic<std::size_t> unresolved_node_lps{0};
    std::atomic<bool> interrupted_search{false};
    auto worker_bounds = std::make_unique<std::atomic<double>[]>(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
        worker_bounds[i].store(std::numeric_limits<double>::infinity(), std::memory_order_relaxed);
    }

    {
        std::vector<std::jthread> workers;
        workers.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([&, i](std::stop_token st) {
                worker_loop(i, root_model, options, queue, incumbent, next_node_id,
                            total_nodes_explored, total_lp_iterations, total_heuristics_found,
                            unresolved_node_lps, interrupted_search, worker_bounds.get(),
                            num_threads, shared_pseudo_costs, start_time, st);
            });
        }
        // A jthread destructor requests stop. Join explicitly so workers can
        // finish the queue protocol before their owners leave scope.
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }

    result.runtime_ms = elapsed_ms();
    result.nodes_explored = total_nodes_explored.load(std::memory_order_relaxed);
    result.lp_iterations = total_lp_iterations.load(std::memory_order_relaxed);
    result.cuts_generated = root_cuts_generated;
    result.heuristics_found = total_heuristics_found.load(std::memory_order_relaxed);

    const bool deadline_reached = options.deadline &&
                                  std::chrono::steady_clock::now() >= *options.deadline;
    const bool frontier_exhausted = queue.empty() && queue.active_workers() == 0;
    const bool search_complete = frontier_exhausted &&
                                 unresolved_node_lps.load(std::memory_order_relaxed) == 0 &&
                                 !interrupted_search.load(std::memory_order_relaxed) &&
                                 !deadline_reached;
    if (incumbent.has_incumbent()) {
        result.primal = incumbent.get_primal();
        result.objective = incumbent.get_objective();

        const double final_lb =
            compute_tree_lower_bound(queue, worker_bounds.get(), num_threads, result.objective);
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
        if (deadline_reached) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "parallel MILP wall-clock deadline reached without incumbent";
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

    return result;
}
}
