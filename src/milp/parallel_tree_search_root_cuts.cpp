#include "parallel_tree_search_internal.hpp"

namespace markov_cero::milp::detail_parallel_tree_search {

void apply_parallel_root_cuts(
    model::Model& root_model, const ParallelOptions& options,
    const NodeLpResult& root_lp, IncumbentManager& incumbent,
    std::atomic<std::size_t>& total_lp_iterations,
    std::vector<double>& current_primal, double& current_obj,
    std::optional<lp::dual::BasisState>& current_basis,
    double& best_lower_bound, std::size_t& root_cuts_generated) {
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

        add_cuts_to_model(root_model, cuts);
        root_cuts_generated = cuts.size();
        const auto cut_lp = solve_node_lp(root_model, options, root_lp.basis);
        total_lp_iterations.fetch_add(cut_lp.iterations, std::memory_order_relaxed);
        if (cut_lp.status != lp::reference::SolveStatus::optimal) return;

        current_primal = cut_lp.primal;
        current_obj = cut_lp.objective;
        current_basis = cut_lp.basis;
        best_lower_bound = std::max(best_lower_bound, cut_lp.lower_bound);
        if (check_integer_feasibility(root_model, current_primal,
                options.feasibility_tolerance, options.integrality_tolerance)) {
            incumbent.update_if_better(current_obj, current_primal,
                                       options.absolute_gap_tolerance);
        }
    } catch (...) {
    }
}

}
