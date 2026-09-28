#include "search_context.hpp"
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
            node_model.variable_lower = node->variable_lower;
            node_model.variable_upper = node->variable_upper;
            if (!node->local_cuts.empty()) {
                add_cuts_to_model(node_model, node->local_cuts);
            }

            node_lp_res =
                solve_node_relaxation(node_model, options, node->warm_basis);
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
