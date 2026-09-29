#include "search_context.hpp"
#include "markov_cero/milp/node_propagation.hpp"
namespace markov_cero::milp::detail {
bool Search::node_relaxation() {
        // Evaluate Node LP relaxation if not root
        node_lp_res = {};
        // Reuse the coefficient storage on ordinary bound-only nodes.
        // A previous local cut changes row structure and requires rematerialization.
        if (node_model.matrix.row_count != root_model.matrix.row_count) node_model = root_model;
        if (node->id == 0) {
            node_lp_res.status = lp::reference::SolveStatus::optimal;
            node_lp_res.primal = current_primal;
            node_lp_res.row_dual = current_row_dual;
            node_lp_res.objective = current_obj;
            node_lp_res.lower_bound = best_lower_bound;
            node_lp_res.basis = current_basis;
        } else {
            node->bounds.materialize(root_model.variable_lower, root_model.variable_upper,
                                     node_model.variable_lower, node_model.variable_upper,
                                     node_bounds_scratch);
            if (!node->local_cuts.empty()) {
                add_cuts_to_model(node_model, node->local_cuts.values());
            }

            const auto propagation = propagate_singleton_rows(
                node_model, node_model.variable_lower, node_model.variable_upper, node->bounds);
            for (const auto& step : propagation.steps) {
                verify::MipObligation note;
                note.kind = verify::MipObligationKind::propagation;
                note.node = node->id;
                note.source_row = step.row;
                note.variable = step.variable;
                note.source_coefficient = step.coefficient;
                note.source_rhs = step.source_rhs;
                note.derived_bound = step.derived_bound;
                note.source_is_lower = step.source_is_lower;
                result.obligations.push_back(std::move(note));
            }
            if (propagation.evidence.usable()) {
                node->lower_bound_evidence = propagation.evidence;
                node->lower_bound = std::max(node->lower_bound, propagation.evidence.value);
            }

            const auto warm_basis = node->warm_basis
                ? std::optional<lp::dual::BasisState>(*node->warm_basis)
                : std::nullopt;
            {
                ScopedSearchTimer timer(result.lp_bound_ms);
                node_lp_res = solve_node_relaxation(node_model, options, warm_basis);
            }
            result.lp_iterations += node_lp_res.iterations;
            ++result.nodes_explored;


            if (node_lp_res.status == lp::reference::SolveStatus::infeasible) {
                // Sound prune: the node's relaxation is empty, so no feasible
                // integer point can live below it.
                return false;
            }
            if (node_lp_res.status != lp::reference::SolveStatus::optimal) {
                // R13/R17 soundness: an *unsolved* node (numerical failure or
                // iteration limit) is NOT prunable — it still carries its
                // inherited bound, and discarding it lets the search exhaust
                // the tree and claim optimality it never proved (the classic
                // bogus "gap 0" on degenerate big-M instances).
                //
                // Retry once cold on the robust primal path, then keep the node
                // open and remember that optimality was not established.

                if (node->lp_failures == 0) {
                    // First failure: retry the node cold on the robust primal
                    // path. Only a node that fails *after* the retry stays open,
                    // so a transient warm-start breakdown does not by itself
                    // forfeit the optimality proof.
                    node->lp_failures = 1;
                    node->warm_basis.reset();
                    queue.push(node);
                    if (queue.capacity_exhausted())
                        stop_reason = "queued-node capacity reached during LP retry";
                } else {
                    ++unsolved_node_lps;
                    min_unsolved_bound = std::min(min_unsolved_bound, node->lower_bound);
                }
                return false;
            }

            const double parent_bound = node->lower_bound;
            node->lower_bound = std::max(node->lower_bound, node_lp_res.lower_bound);

            // Bound pruning after solving node LP
            if (node_lp_res.lower_bound >= best_upper_bound - options.absolute_gap_tolerance) {
                return false;
            }

            // Update pseudo-cost of parent branch
            if (node->depth > 0) {
                const std::size_t b_var = node->branch_variable;
                const double delta_z = node_lp_res.lower_bound - parent_bound;
                const double frac = node->branch_value - std::floor(node->branch_value);
                if (node->is_down_branch) {
                    pseudo_costs[b_var].record_down(delta_z, frac);
                } else {
                    pseudo_costs[b_var].record_up(delta_z, 1.0 - frac);
                }
            }
        }


return true;
}
}
