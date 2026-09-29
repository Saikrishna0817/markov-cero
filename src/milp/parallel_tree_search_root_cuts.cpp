#include "parallel_tree_search_internal.hpp"

namespace markov_cero::milp::detail_parallel_tree_search {

void apply_parallel_root_cuts(
    model::Model& root_model, const ParallelOptions& options,
    const NodeLpResult& root_lp, IncumbentManager& incumbent,
    std::atomic<std::size_t>& total_lp_iterations,
    std::vector<double>& current_primal, double& current_obj,
    std::optional<lp::dual::BasisState>& current_basis,
    double& best_lower_bound, std::size_t& root_cuts_generated,
    ProofEventCollector& proof_events) {
    if (!options.enable_cuts || !root_lp.basis.has_value()) return;
    try {
        const auto canonical =
            transform::sparse_canonicalize(root_model, /*relax_integrality=*/true);
        auto cuts = generate_gomory_cuts(root_model, current_primal, canonical,
                                         *root_lp.basis, options.max_cut_rounds);
        if (options.enable_mir_cuts) {
            auto mir_cuts = generate_mir_cuts(root_model, current_primal, canonical,
                                              *root_lp.basis, options.max_cut_rounds);
            cuts.insert(cuts.end(), mir_cuts.begin(), mir_cuts.end());
        }
        cuts = filter_cuts(std::move(cuts), options.max_cut_rounds);
        if (cuts.empty()) return;

        model::Model candidate_model = root_model;
        add_cuts_to_model(candidate_model, cuts);
        const auto cut_lp = solve_node_lp(candidate_model, options, root_lp.basis);
        total_lp_iterations.fetch_add(cut_lp.iterations, std::memory_order_relaxed);
        if (cut_lp.status != lp::reference::SolveStatus::optimal) {
            std::fprintf(stderr, "parallel root cuts reverted: LP status %d\n",
                         static_cast<int>(cut_lp.status));
            return;
        }
        const auto rounded_cut = rounded_integer_candidate(root_model, cut_lp.primal,
            options.feasibility_tolerance, options.integrality_tolerance);

        root_model = std::move(candidate_model);
        root_cuts_generated = cuts.size();
        // Root cuts prune the entire tree, so they must appear in the proof
        // ledger. The separation point is the pre-cut-round LP primal, whose
        // dimension matches the cut coefficients (both over original columns).
        proof_events.record_cuts(proof_events.root, 0, cuts, root_lp.primal);
        current_primal = cut_lp.primal;
        current_obj = cut_lp.objective;
        current_basis = cut_lp.basis;
        best_lower_bound = std::max(best_lower_bound, cut_lp.lower_bound);
        if (rounded_cut.found) {
            incumbent.update_if_better(rounded_cut.objective, rounded_cut.primal,
                                       options.absolute_gap_tolerance);
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "parallel root cuts skipped: %s\n", error.what());
    } catch (...) {
        std::fprintf(stderr, "parallel root cuts skipped: unknown exception\n");
    }
}

NodeLpResult solve_parallel_node_with_cuts(
    BranchNode& node, const model::Model& root_model, const ParallelOptions& options,
    const std::optional<lp::dual::BasisState>& warm_basis,
    const std::vector<model::Bound>& lower, const std::vector<model::Bound>& upper,
    std::size_t explored_count, std::atomic<std::size_t>& total_lp_iterations,
    std::atomic<std::size_t>& total_cuts_generated) {
    // Each worker owns its popped node and this model copy. Only counters are shared.
    model::Model local_model;
    NodeLpResult solution;
    if (node.local_cuts.empty()) {
        solution = solve_node_lp(root_model, options, warm_basis, lower, upper);
    } else {
        local_model = root_model;
        local_model.variable_lower = lower;
        local_model.variable_upper = upper;
        add_cuts_to_model(local_model, node.local_cuts.values());
        solution = solve_node_lp(local_model, options, warm_basis);
    }
    total_lp_iterations.fetch_add(solution.iterations, std::memory_order_relaxed);
    const Options policy;
    if (!options.enable_cuts || policy.separation_frequency == 0 || node.id == 0 ||
        explored_count % policy.separation_frequency != 0 ||
        solution.status != lp::reference::SolveStatus::optimal || !solution.basis ||
        solution.iterations > policy.separation_max_node_iterations) return solution;
    double max_fractionality = 0.0;
    for (const auto variable : find_fractional_variables(
             solution.primal, root_model.variable_type, options.integrality_tolerance)) {
        const double f = solution.primal[variable] - std::floor(solution.primal[variable]);
        max_fractionality = std::max(max_fractionality, std::min(f, 1.0 - f));
    }
    if (max_fractionality < policy.separation_min_max_fractionality) return solution;
    if (node.local_cuts.empty()) {
        local_model = root_model;
        local_model.variable_lower = lower;
        local_model.variable_upper = upper;
    }
    const std::size_t rounds = std::min(policy.max_in_tree_cut_rounds, options.max_cut_rounds);
    for (std::size_t round = 0; round < rounds; ++round) {
        try {
            const auto canonical = transform::sparse_canonicalize(local_model, true);
            auto candidates = generate_gomory_cuts(local_model, solution.primal, canonical,
                *solution.basis, policy.max_in_tree_cuts_per_node);
            if (options.enable_mir_cuts) {
                auto mir = generate_mir_cuts(local_model, solution.primal, canonical,
                    *solution.basis, policy.max_in_tree_cuts_per_node);
                candidates.insert(candidates.end(), mir.begin(), mir.end());
            }
            candidates = filter_cuts(std::move(candidates), policy.max_in_tree_cuts_per_node);
            std::vector<Cut> fresh;
            for (auto& cut : candidates) {
                double lhs = 0.0;
                for (std::size_t j = 0; j < cut.coefficients.size(); ++j)
                    lhs += cut.coefficients[j] * solution.primal[j];
                if (cut.rhs - lhs < 1e-4) continue;
                bool duplicate = false;
                for (const auto& existing : node.local_cuts.values()) {
                    if (compute_cosine_similarity(cut, existing) > 0.95) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) fresh.push_back(std::move(cut));
            }
            if (fresh.empty()) break;
            model::Model candidate_model = local_model;
            add_cuts_to_model(candidate_model, fresh);
            const auto base_iterations = solution.iterations;
            const auto cut_lp = solve_node_lp(candidate_model, options, solution.basis, lower, upper);
            total_lp_iterations.fetch_add(cut_lp.iterations, std::memory_order_relaxed);
            if (cut_lp.status != lp::reference::SolveStatus::optimal) {
                std::fprintf(stderr, "parallel in-tree cuts reverted at node %zu: LP status %d\n",
                             node.id, static_cast<int>(cut_lp.status));
                break;
            }
            node.local_cuts.append(fresh);
            if (node.local_cuts.size() > policy.max_pool_cuts) {
                node.local_cuts.sort_by_violation();
                node.local_cuts.truncate(policy.max_pool_cuts);
            }
            total_cuts_generated.fetch_add(fresh.size(), std::memory_order_relaxed);
            local_model = std::move(candidate_model);
            solution = cut_lp;
            if (static_cast<double>(cut_lp.iterations) >
                policy.separation_lp_budget_factor * static_cast<double>(base_iterations)) break;
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "parallel in-tree cuts skipped at node %zu: %s\n",
                         node.id, error.what());
            break;
        }
    }
    return solution;
}

}
