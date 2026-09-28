#include "search_context.hpp"
namespace markov_cero::milp::detail {
bool Search::branch() {
        node->bounds.materialize(root_model.variable_lower, root_model.variable_upper,
                                 node_model.variable_lower, node_model.variable_upper,
                                 node_bounds_scratch);
        // Check integer feasibility of node solution
        const auto fractional_vars = find_fractional_variables(
            node_lp_res.primal, root_model.variable_type, options.integrality_tolerance);

        if (fractional_vars.empty()) {
            // Integer feasible incumbent found!

            if (node_lp_res.objective < best_upper_bound) {
                best_upper_bound = node_lp_res.objective;
                best_primal = node_lp_res.primal;
            }
            return false;
        }

        // Try quick simple rounding on fractional point
        if (options.enable_heuristics && result.nodes_explored % 5 == 0) {
            const auto hr =
                simple_rounding(node_model, node_lp_res.primal, options.feasibility_tolerance,
                                options.integrality_tolerance);
            if (hr.found && hr.objective < best_upper_bound) {
                best_upper_bound = hr.objective;
                best_primal = hr.primal;
                ++result.heuristics_found;
            }
        }

        // 7. Branching Variable Selection
        std::size_t branch_var = root_model.matrix.column_count;
        auto& current_node_model = node_model;
        if (options.branching_strategy == BranchingStrategy::strong_branching &&
            node_lp_res.basis.has_value()) {
            try {
                StrongBranchingOptions sb_opts;
                sb_opts.integrality_tolerance = options.integrality_tolerance;
                sb_opts.feasibility_tolerance = options.feasibility_tolerance;
                sb_opts.deadline = options.deadline;
#ifdef MARKOV_CERO_ENABLE_ML
                // Strong-branch probes create labels, not prior observations
                // for the feature vector. Do not inject these labels into
                // pseudo-cost history while collecting training records.
                sb_opts.update_pseudo_costs = sb_log_file == nullptr;
#else
                sb_opts.update_pseudo_costs = true;
#endif
                // The graph features must describe the state available before
                // this node's strong-branch probes. Those probes update the
                // pseudo-cost table with the very gains used as labels; using
                // the updated table below would leak the target into training.
                const auto feature_pseudo_costs = pseudo_costs;
                const auto sb_res = evaluate_strong_branching(
                    current_node_model, node_lp_res.primal, node_lp_res.objective,
                    node_lp_res.basis, sb_opts, &pseudo_costs);
                if (sb_res.deadline_reached) {
                    queue.push(node);
                    stop_reason = "time limit reached during strong branching";
                    return false;
                }

                if (sb_res.subproblem_infeasible) {
                    return false; // Prune node
                }
                branch_var = sb_res.best_variable;
#ifdef MARKOV_CERO_ENABLE_ML
                // W2/D-05: strong-branching training log. When the collector
                // sets MARKOV_CERO_SB_LOG, append one record per node: the
                // per-candidate features and Achterberg product scores, but
                // only for candidates whose two child LPs were resolved. An
                // iteration-limited child has no certified degradation and
                // must not be mislabeled as a zero-gain branch.
                if (sb_log_file != nullptr) {
                    std::vector<std::size_t> labeled_candidates;
                    std::vector<double> sb_scores;
                    labeled_candidates.reserve(sb_res.candidates.size());
                    sb_scores.reserve(sb_res.candidates.size());
                    for (const auto& c : sb_res.candidates) {
                        if (c.down_resolved && c.up_resolved && std::isfinite(c.score)) {
                            labeled_candidates.push_back(c.variable_index);
                            sb_scores.push_back(c.score);
                        }
                    }
                    if (!labeled_candidates.empty()) {
                        const auto graph = extract_bipartite_features(
                            node_lp_res.primal, current_node_model.variable_type,
                            labeled_candidates, feature_pseudo_costs, current_node_model,
                            node_lp_res.row_dual);
                        ml::append_sb_record(*sb_log_file, graph, sb_scores);
                    }
                }
#endif
            } catch (const std::bad_alloc&) {
                throw;
            } catch (const std::exception& e) {

                branch_var = select_branching_variable(node_lp_res.primal, root_model.variable_type,
                                                       pseudo_costs, options.branching_strategy,
                                                       options.integrality_tolerance,
                                                       &current_node_model,
                                                       branching_scorer,
                                                       result.ml_requested ? &result.ml_telemetry
                                                                           : nullptr,
                                                       &node_lp_res.row_dual);
            }
        } else {
            branch_var = select_branching_variable(node_lp_res.primal, root_model.variable_type,
                                                   pseudo_costs, options.branching_strategy,
                                                   options.integrality_tolerance,
                                                   &current_node_model,
                                                   branching_scorer,
                                                   result.ml_requested ? &result.ml_telemetry
                                                                       : nullptr,
                                                   &node_lp_res.row_dual);
        }

        if (branch_var >= root_model.matrix.column_count) {
            return false;
        }

        const double branch_val = node_lp_res.primal[branch_var];
        const double floor_val = std::floor(branch_val);
        const double ceil_val = std::ceil(branch_val);
        const auto shared_warm_basis = node_lp_res.basis
            ? std::make_shared<const lp::dual::BasisState>(*node_lp_res.basis)
            : std::shared_ptr<const lp::dual::BasisState>{};

        // Child 1 (Down Branch): x_k <= floor_val
        bool down_valid = true;
        if (current_node_model.variable_lower[branch_var].is_finite() &&
            floor_val < current_node_model.variable_lower[branch_var].value - 1e-9) {
            down_valid = false;
        }
        if (down_valid) {
            auto down_child = std::make_shared<BranchNode>();
            down_child->id = next_node_id++;
            down_child->parent_id = node->id;
            down_child->depth = node->depth + 1;
            down_child->lower_bound =
                node_lp_res.lower_bound; // conservative dual bound is valid for descendants
            down_child->branch_variable = branch_var;
            down_child->branch_value = branch_val;
            down_child->is_down_branch = true;
            down_child->bounds = node->bounds.with_upper(branch_var, model::Bound::finite(floor_val));
            down_child->warm_basis = shared_warm_basis;
            down_child->local_cuts = node->local_cuts;
            queue.push(down_child);
            if (queue.capacity_exhausted()) stop_reason = "queued-node capacity reached";
        }

        // Child 2 (Up Branch): x_k >= ceil_val
        bool up_valid = true;
        if (current_node_model.variable_upper[branch_var].is_finite() &&
            ceil_val > current_node_model.variable_upper[branch_var].value + 1e-9) {
            up_valid = false;
        }
        if (up_valid) {
            auto up_child = std::make_shared<BranchNode>();
            up_child->id = next_node_id++;
            up_child->parent_id = node->id;
            up_child->depth = node->depth + 1;
            up_child->lower_bound = node_lp_res.lower_bound;
            up_child->branch_variable = branch_var;
            up_child->branch_value = branch_val;
            up_child->is_down_branch = false;
            up_child->bounds = node->bounds.with_lower(branch_var, model::Bound::finite(ceil_val));
            up_child->warm_basis = shared_warm_basis;
            up_child->local_cuts = node->local_cuts;
            queue.push(up_child);
            if (queue.capacity_exhausted()) stop_reason = "queued-node capacity reached";
        }


return true;
}
}
