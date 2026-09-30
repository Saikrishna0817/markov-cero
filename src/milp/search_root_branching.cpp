#include "search_context.hpp"
namespace markov_cero::milp::detail {
bool Search::root_branching() {
    // 4. Root strong branching is only useful for strategies that consume its
    // scores. Running it unconditionally made an ordinary pseudo-cost solve
    // spend O(number of fractional variables) full LP re-solves before its
    // first B&B node (the supply-chain case spent >90s here). Bound the root
    // probe set; later strong-branching decisions remain governed by strategy.
    const bool root_uses_strong_branching =
        options.branching_strategy == BranchingStrategy::strong_branching ||
        options.branching_strategy == BranchingStrategy::reliability;
    if (options.enable_strong_branching && root_uses_strong_branching &&
        current_basis.has_value()) {
        try {
            StrongBranchingOptions sb_opts;
            sb_opts.integrality_tolerance = options.integrality_tolerance;
            sb_opts.feasibility_tolerance = options.feasibility_tolerance;
            sb_opts.deadline = options.deadline;
#ifdef MARKOV_CERO_ENABLE_ML
            // During data collection, keep probe-only gains out of the
            // pseudo-cost state. Training features should reflect only
            // information available to the ML policy from solved tree nodes.
            sb_opts.update_pseudo_costs = sb_log_file == nullptr;
#else
            sb_opts.update_pseudo_costs = true;
#endif
            // The root pass initializes pseudo-costs only; feature/label
            // records are emitted for per-node strong branching below.
            sb_opts.max_candidates = 4;
            const auto sb_res = evaluate_strong_branching(root_model, current_primal, current_obj,
                                                          current_basis, sb_opts, &pseudo_costs);
            if (sb_res.deadline_reached) {
                result.status = lp::reference::SolveStatus::resource_limit;
                result.best_bound = best_lower_bound;
                if (std::isfinite(best_upper_bound)) {
                    result.primal = best_primal;
                    result.objective = best_upper_bound;
                    result.relative_gap = std::max(0.0, best_upper_bound - best_lower_bound) /
                                          std::max(1.0, std::abs(best_upper_bound));
                }
                result.message = "wall-clock deadline reached during root strong branching";
                result.runtime_ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start_time).count();
                return false;
            }


            if (sb_res.subproblem_infeasible) {
                result.status = lp::reference::SolveStatus::infeasible;
                result.message = "proven infeasible by strong branching at root";
                const auto elapsed = std::chrono::steady_clock::now() - start_time;
                result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
                return false;
            }

            // Apply discovered domain reductions to root model. These bounds
            // are logical consequences of an infeasible branch, but they can
            // invalidate the root LP point and basis. Re-solve before the root
            // node is enqueued; using the stale relaxation can make both
            // branches appear infeasible and incorrectly prune a feasible MIP.
            bool root_domain_changed = false;
            for (const auto& dr : sb_res.domain_reductions) {
                if (dr.variable_index < root_model.matrix.column_count) {

                    if (dr.new_lower.is_finite()) {
                        root_domain_changed = root_domain_changed ||
                            !root_model.variable_lower[dr.variable_index].is_finite() ||
                            dr.new_lower.value > root_model.variable_lower[dr.variable_index].value;
                        root_model.variable_lower[dr.variable_index] = dr.new_lower;
                    }
                    if (dr.new_upper.is_finite()) {
                        root_domain_changed = root_domain_changed ||
                            !root_model.variable_upper[dr.variable_index].is_finite() ||
                            dr.new_upper.value < root_model.variable_upper[dr.variable_index].value;
                        root_model.variable_upper[dr.variable_index] = dr.new_upper;
                    }
                }
            }
            if (root_domain_changed) {
                const auto reduced_root_lp =
                    solve_node_relaxation(root_model, options, current_basis);
                result.lp_iterations += reduced_root_lp.iterations;
                if (reduced_root_lp.status != lp::reference::SolveStatus::optimal) {
                    result.status = reduced_root_lp.status;
                    if (std::isfinite(reduced_root_lp.lower_bound))
                        result.best_bound = reduced_root_lp.lower_bound;
                    result.message = reduced_root_lp.status == lp::reference::SolveStatus::infeasible
                        ? "root relaxation is infeasible after certified strong-branching reductions"
                        : "root relaxation failed after strong-branching reductions: " +
                              reduced_root_lp.message;
                    const auto elapsed = std::chrono::steady_clock::now() - start_time;
                    result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
                    return false;
                }
                current_primal = reduced_root_lp.primal;
                current_row_dual = reduced_root_lp.row_dual;
                current_obj = reduced_root_lp.objective;
                current_basis = reduced_root_lp.basis;
                best_lower_bound = std::max(best_lower_bound, reduced_root_lp.lower_bound);

            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& e) {

        }
    }

    // 5. Initialize Active Node Priority Queue (policy-ordered heap with a
    // policy-independent min-bound query — see NodeFrontier).
    queue = NodeFrontier{options.node_selection};
    queue.set_maximum_size(options.max_queued_nodes);

    // R13/R17 soundness bookkeeping. `unsolved_node_lps` counts nodes whose
    // relaxation could not be certified; `min_unsolved_bound` is the weakest
    // bound among them, so the honest global lower bound is
    // min(best_lower_bound, min_unsolved_bound) whenever the counter is nonzero.
    unsolved_node_lps = 0;
    min_unsolved_bound = std::numeric_limits<double>::infinity();

    // RES-01 (resource contract section 4): the frontier is solver-owned
    // memory, so the shared budget is charged before the root node is
    // allocated. A refusal stops the search with a resource outcome and only
    // whatever bound/incumbent was already established.
    if (options.context && !options.context->charge_or_stop(sizeof(BranchNode))) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.best_bound = best_lower_bound;
        if (std::isfinite(best_upper_bound)) {
            result.primal = best_primal;
            result.objective = best_upper_bound;
            result.relative_gap = std::max(0.0, best_upper_bound - best_lower_bound) /
                                  std::max(1.0, std::abs(best_upper_bound));
        }
        result.message = "memory budget refused the root node allocation";
        result.runtime_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start_time).count();
        return false;
    }
    auto root_node = std::make_shared<BranchNode>();
    root_node->id = 0;
    root_node->parent_id = 0;
    root_node->depth = 0;
    root_node->lower_bound = best_lower_bound;
    if (current_basis) {
        root_node->warm_basis = std::make_shared<const lp::dual::BasisState>(*current_basis);
    }
    queue.push(root_node);
    if (queue.capacity_exhausted()) stop_reason = "queued-node capacity reached";


return true;
}
}
