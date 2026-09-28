#include "search_context.hpp"
namespace markov_cero::milp::detail {
bool Search::separate_cuts() {
        // In-tree cut separation: frequency-gated, pool-deduped, bounded rounds (ED-005).
        // RW-1 safety valves: (a) skip expensive nodes (LP iterations above
        // separation_max_node_iterations) — re-separating on hard nodes costs more
        // LP re-solve time than the pruning recovers; (b) skip near-integral nodes
        // whose max fractionality is tiny — branching closes them cheaply while cut
        // rows would inflate every descendant LP (both measured on flugpl).
        double max_fractionality = 0.0;
        for (const auto fv : find_fractional_variables(node_lp_res.primal,
                                                       root_model.variable_type,
                                                       options.integrality_tolerance)) {
            const double f = node_lp_res.primal[fv] - std::floor(node_lp_res.primal[fv]);
            max_fractionality = std::max(max_fractionality, std::min(f, 1.0 - f));
        }
        // Per-node budget: the cut round may spend at most
        // separation_lp_budget_factor x this node's base LP iterations.
        const double node_lp_budget =
            static_cast<double>(node_lp_res.iterations) * options.separation_lp_budget_factor;
        if (options.enable_cuts && options.separation_frequency > 0 && node->id != 0 &&
            node_lp_res.basis.has_value() &&
            node_lp_res.iterations <= options.separation_max_node_iterations &&
            max_fractionality >= options.separation_min_max_fractionality &&
            result.nodes_explored % options.separation_frequency == 0) {
            try {
                const std::size_t rounds =
                    std::min(options.max_in_tree_cut_rounds, options.max_cut_rounds);
                for (std::size_t round = 0; round < rounds; ++round) {
                    const auto fractional = find_fractional_variables(
                        node_lp_res.primal, root_model.variable_type,
                        options.integrality_tolerance);
                    if (fractional.empty()) {
                        break;
                    }

                    const auto canon = transform::sparse_canonicalize(
                        node_model, /*relax_integrality=*/true);
                    // In-tree budget: fewer, stronger cuts per node. Generating the
                    // full root allowance at every node inflates LP size (and the
                    // canonicalization + re-solve cost) faster than it prunes —
                    // the measured flugpl regression (RW-1 tuning, ED-005).
                    std::vector<Cut> candidates = generate_gomory_cuts(
                        node_model, node_lp_res.primal, canon, *node_lp_res.basis,
                        options.max_in_tree_cuts_per_node);
                    if (options.enable_mir_cuts) {
                        const auto mir_cuts = generate_mir_cuts(
                            node_model, node_lp_res.primal, canon, *node_lp_res.basis,
                            options.max_in_tree_cuts_per_node);
                        candidates.insert(candidates.end(), mir_cuts.begin(), mir_cuts.end());
                    }
                    candidates = filter_cuts(std::move(candidates),
                                             options.max_in_tree_cuts_per_node);

                    std::vector<Cut> fresh;
                    fresh.reserve(candidates.size());
                    for (auto& cut : candidates) {
                        double lhs = 0.0;
                        for (std::size_t j = 0; j < cut.coefficients.size(); ++j) {
                            lhs += cut.coefficients[j] * node_lp_res.primal[j];
                        }
                        if (cut.rhs - lhs < 1e-4) {
                            continue;
                        }
                        bool pooled = false;
                        for (const auto& existing : root_cut_list) {
                            if (compute_cosine_similarity(cut, existing) > 0.95) {
                                pooled = true;
                                break;
                            }
                        }
                        if (!pooled) {
                            for (const auto& existing : node->local_cuts.values()) {
                                if (compute_cosine_similarity(cut, existing) > 0.95) {
                                    pooled = true;
                                    break;
                                }
                            }
                        }
                        if (!pooled) {
                            fresh.push_back(std::move(cut));
                        }
                    }
                    if (fresh.empty()) {
                        break;
                    }

                    add_cuts_to_model(node_model, fresh);
                    const auto prior_cut_count = node->local_cuts.size();
                    node->local_cuts.append(fresh);
                    result.cuts_generated += fresh.size();

                    const auto cut_lp =
                        solve_node_relaxation(node_model, options, node_lp_res.basis);
                    result.lp_iterations += cut_lp.iterations;
                    if (cut_lp.status != lp::reference::SolveStatus::optimal) {
                        // Unverified cuts must not propagate to children: revert this round.
                        node->local_cuts.truncate(prior_cut_count);
                        result.cuts_generated -= fresh.size();
                        break;
                    }
                    // Budget enforcement: if the re-solve consumed more than the
                    // node's allowance, keep the (verified) cuts but stop separating
                    // here. Prevents unbounded LP-work amplification on instances
                    // where in-tree cuts do not pay for themselves.
                    if (static_cast<double>(cut_lp.iterations) > node_lp_budget) {
                        break;
                    }
                    node_lp_res = cut_lp;
                    node->lower_bound = std::max(node->lower_bound, cut_lp.lower_bound);
                    if (cut_lp.lower_bound >= best_upper_bound - options.absolute_gap_tolerance) {
                        break;
                    }
                }
            } catch (const std::bad_alloc&) {
                throw;
            } catch (const std::exception& e) {

            }

            if (node->local_cuts.size() > options.max_pool_cuts) {
                node->local_cuts.sort_by_violation();
                node->local_cuts.truncate(options.max_pool_cuts);
            }
        }


return true;
}
}
