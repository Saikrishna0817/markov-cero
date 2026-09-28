#include "search_context.hpp"
namespace markov_cero::milp::detail {
bool Search::root_relaxation() {
    // 1. Solve Root Continuous LP Relaxation
    root_lp = solve_node_relaxation(root_model, options, std::nullopt);
    result.lp_iterations += root_lp.iterations;
    result.condition_estimate = root_lp.condition_estimate;
    result.nodes_explored = 1;

    if (root_lp.status == lp::reference::SolveStatus::infeasible) {
        result.status = lp::reference::SolveStatus::infeasible;
        result.message = "root continuous relaxation is infeasible";
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        return false;
    }
    if (root_lp.status != lp::reference::SolveStatus::optimal) {
        result.status = root_lp.status;
        if (std::isfinite(root_lp.lower_bound))
            result.best_bound = root_lp.lower_bound;
        result.message = root_lp.message.empty()
                             ? std::string("root continuous relaxation failed: ") +
                                   lp::reference::to_string(root_lp.status)
                             : "root continuous relaxation failed: " + root_lp.message;
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        return false;
    }

    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
        result.status = lp::reference::SolveStatus::resource_limit;
        result.best_bound = root_lp.lower_bound;
        result.message = "wall-clock deadline reached during root relaxation";
        if (check_integer_feasibility(root_model, root_lp.primal,
                                      options.feasibility_tolerance,
                                      options.integrality_tolerance)) {
            result.primal = root_lp.primal;
            result.objective = root_lp.objective;
            result.relative_gap = relative_gap(root_lp.objective, root_lp.lower_bound);
        }
        result.runtime_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start_time).count();
        return false;
    }

    best_lower_bound = root_lp.lower_bound;

    // Check if root continuous solution is integer feasible
    if (check_integer_feasibility(root_model, root_lp.primal, options.feasibility_tolerance,
                                  options.integrality_tolerance)) {
        result.primal = root_lp.primal;
        result.objective = root_lp.objective;
        result.best_bound = root_lp.lower_bound;
        result.relative_gap = relative_gap(root_lp.objective, root_lp.lower_bound);
        result.status = result.relative_gap <= options.relative_gap_tolerance
                            ? gap_status(result.objective, result.best_bound, options.absolute_gap_tolerance)
                            : lp::reference::SolveStatus::iteration_limit;
        result.message = result.status == lp::reference::SolveStatus::optimal
                             ? "root relaxation integer feasible and dual gap certified"
                             : "root relaxation integer feasible but dual gap is not closed";
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
        return false;
    }

    // 2. Run Primal Heuristics at Root
    if (options.enable_heuristics) {
        const auto hr = simple_rounding(root_model, root_lp.primal, options.feasibility_tolerance,
                                        options.integrality_tolerance);
        if (hr.found && hr.objective < best_upper_bound) {
            best_upper_bound = hr.objective;
            best_primal = hr.primal;
            ++result.heuristics_found;
        }

        const auto fp =
            feasibility_pump(root_model, root_lp.primal, options.max_pump_iterations,
                             options.feasibility_tolerance, options.integrality_tolerance);
        if (fp.found && fp.objective < best_upper_bound) {
            best_upper_bound = fp.objective;
            best_primal = fp.primal;
            ++result.heuristics_found;
        }
    }

    // 3. Generate Gomory Mixed-Integer and MIR Cuts at Root
    current_basis = root_lp.basis;
    current_primal = root_lp.primal;
    current_row_dual = root_lp.row_dual;
    current_obj = root_lp.objective;
    root_cut_list = {};

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
                root_cut_list = cuts;
                result.cuts_generated += cuts.size();

                // Re-solve root LP with cuts
                const auto cut_lp =
                    solve_node_relaxation(root_model, options, root_lp.basis);
                result.lp_iterations += cut_lp.iterations;
                if (cut_lp.status == lp::reference::SolveStatus::optimal) {
                    current_primal = cut_lp.primal;
                    current_row_dual = cut_lp.row_dual;
                    current_obj = cut_lp.objective;
                    current_basis = cut_lp.basis;
                    best_lower_bound = std::max(best_lower_bound, cut_lp.lower_bound);

                    if (check_integer_feasibility(root_model, current_primal,
                                                  options.feasibility_tolerance,
                                                  options.integrality_tolerance)) {
                        if (current_obj < best_upper_bound) {
                            best_upper_bound = current_obj;
                            best_primal = current_primal;
                        }
                    }
                }
            }
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& e) {

        }
    }

    // Check if root cuts closed the optimality gap
    if (!best_primal.empty() && best_lower_bound > -std::numeric_limits<double>::infinity()) {
        const double gap = relative_gap(best_upper_bound, best_lower_bound);
        if (gap <= options.relative_gap_tolerance) {
            result.status = gap_status(best_upper_bound, best_lower_bound, options.absolute_gap_tolerance);
            result.primal = best_primal;
            result.objective = best_upper_bound;
            result.best_bound = best_lower_bound;
            result.relative_gap = gap;
            result.message = "optimality gap closed at root node";
            const auto elapsed = std::chrono::steady_clock::now() - start_time;
            result.runtime_ms = std::chrono::duration<double, std::milli>(elapsed).count();
            return false;
        }
    }


return true;
}
}
