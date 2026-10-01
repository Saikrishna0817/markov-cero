#include "minlp_solver_internal.hpp"
namespace markov_cero::minlp {
using namespace detail_minlp_solver;

namespace {
// MINLP-01 (minlp-oa.md §7.1): statuses whose best_bound the MILP assurance
// layer certifies (milp-node-bounds §3/§6), early stops included.
bool certified_master_status(lp::reference::SolveStatus status) {
    return status == lp::reference::SolveStatus::optimal ||
           status == lp::reference::SolveStatus::gap_satisfied ||
           status == lp::reference::SolveStatus::feasible ||
           status == lp::reference::SolveStatus::iteration_limit ||
           status == lp::reference::SolveStatus::resource_limit;
}
} // namespace

MinlpSolution iterate_outer_approximation(const MinlpProblem& problem, const NlpModel& nlp,
    const std::vector<double>& x0, const MinlpOptions& options) {
    MinlpSolution out;
    OaRowAccumulators acc;
    const auto finalize = [&]() {
        out.cuts_added = acc.cuts.size();
        out.oa_cuts = std::move(acc.cuts);
        return std::move(out);
    };
    const std::vector<OaRowSource> row_map = oa_ineq_source_map(*problem.source_model);
    if (row_map.size() != nlp.n_ineq) {
        out.status = lp::reference::SolveStatus::invalid_model;
        out.message = "minlp: source row map does not match the NLP inequality rows";
        return finalize();
    }

    double best_obj = kInf;
    std::vector<double> best_x;
    double best_bound = -kInf;

    const auto add_rows_at = [&](const std::vector<double>& p, double f_at_p) {
        return append_oa_rows(*problem.source_model, nlp, row_map, p, f_at_p, acc,
                              out.cuts_replayed);
    };

    std::vector<double> x = x0;
    std::size_t consecutive_sqp_failures = 0;
    for (std::size_t iter = 0; iter < options.max_iterations; ++iter) {
        if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
            out.status = lp::reference::SolveStatus::resource_limit;
            out.message = "minlp outer-approximation wall-clock deadline reached";
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }
        out.iterations = iter + 1;

        // 1) NLP subproblem. Iteration 1 (and any iteration where the master
        // has no usable integer point): relaxed start. Afterwards the master's
        // integer solution pins the integer variables. Fixing them to x0 on
        // iteration zero would constrain an arbitrary (often fractional)
        // starting guess and is not a valid relaxation step.
        NlpModel fixed_model = iter == 0
                                  ? nlp
                                  : with_fixed_integers(nlp, problem.integer_indices, x);
        const auto sub = nlp::solve_sqp(fixed_model, x, options.sqp_options);
        ++out.sqp_calls;
        if (sub.status == lp::reference::SolveStatus::numerical_failure) {
            // OA tolerates failed subproblems: rows at the current point are
            // still valid cuts (minlp-oa.md §10 case F). Abort only after
            // repeated failures; the failure counters are totals (§8.1).
            ++out.sqp_failures;
            ++consecutive_sqp_failures;
            if (consecutive_sqp_failures > 3) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: NLP subproblem failed repeatedly: " + sub.message;
                retain_incumbent(out, best_x, best_obj, best_bound);
                return finalize();
            }
        } else {
            consecutive_sqp_failures = 0;
        }

        const std::vector<double> point =
            sub.x.empty() ? x : sub.x;
        if (point.size() != nlp.n_vars) {
            out.status = lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: NLP subproblem returned a primal with the wrong dimension";
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }
        const double point_obj = nlp.eval_objective(point);
        if (!std::isfinite(point_obj)) {
            out.status = lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: NLP subproblem returned a non-finite objective";
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }

        // 2) Incumbent (contract §6): original feasibility through the
        // independent verifier, integrality, and a clean optimal subproblem.
        bool integer_ok = true;
        for (std::size_t idx : problem.integer_indices) {
            if (std::abs(point[idx] - std::round(point[idx])) > 1e-6) {
                integer_ok = false;
                break;
            }
        }
        const nlp::NlpFeasibilityReport original_feasible =
            nlp::verify_nlp_feasibility(nlp, point, options.feasibility_tolerance);
        const double actual_objective = nlp.eval_objective(point);
        if (integer_ok && sub.status == lp::reference::SolveStatus::optimal &&
            original_feasible.feasible && std::isfinite(actual_objective)) {
            if (actual_objective < best_obj) {
                best_obj = actual_objective;
                best_x = point;
                out.integer_feasible = true;
            }
        }

        // OA row cap (contract §8.2): stop like the iteration limit.
        if (acc.cuts.size() + 1 + nlp.n_ineq > options.max_oa_cuts) {
            out.status = lp::reference::SolveStatus::iteration_limit;
            out.message = "minlp: OA row cap max_oa_cuts=" +
                          std::to_string(options.max_oa_cuts) +
                          " reached before refinement; incumbent retained without an "
                          "optimality claim";
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }

        // 3) Refine the outer approximation at the subproblem point; the new
        // cuts are provenance-recorded and replayed before the master runs.
        if (const std::string cut_error = add_rows_at(point, point_obj); !cut_error.empty()) {
            out.status = lp::reference::SolveStatus::numerical_failure;
            out.message = cut_error;
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }

        // 4) Solve the master MILP (existing engine, unmodified per D-03).
        model::Model master = build_master(nlp, problem.integer_indices,
                                           acc.obj_grads, acc.obj_rhs,
                                           acc.cut_grads, acc.cut_rhs);
        try {
            master.validate();
        } catch (const std::exception& e) {
            out.status = lp::reference::SolveStatus::invalid_model;
            out.message = std::string("minlp: master model invalid: ") + e.what();
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }
        milp::Options milp_opts;
        milp_opts.max_nodes = options.milp_max_nodes;
        milp_opts.time_limit_seconds = options.milp_time_limit;
        if (options.deadline) {
            const double remaining_seconds = std::chrono::duration<double>(
                *options.deadline - std::chrono::steady_clock::now()).count();
            if (remaining_seconds <= 0.0) {
                out.status = lp::reference::SolveStatus::resource_limit;
                out.message = "minlp deadline reached before OA master solve";
                retain_incumbent(out, best_x, best_obj, best_bound);
                return finalize();
            }
            milp_opts.time_limit_seconds = std::min(milp_opts.time_limit_seconds,
                                                    remaining_seconds);
        }
        milp_opts.enable_cuts = false;   // OA rows are the cuts; keep master lean
        milp_opts.enable_heuristics = false;
        milp_opts.enable_strong_branching = false;
        const auto master_sol = milp::solve(master, milp_opts);
        out.master_nodes += master_sol.nodes_explored;

        if (master_sol.status == lp::reference::SolveStatus::infeasible) {
            if (out.integer_feasible) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: master reported infeasible despite a verified incumbent; "
                              "global optimality cannot be certified";
                retain_incumbent(out, best_x, best_obj, best_bound);
                return finalize();
            }
            out.status = lp::reference::SolveStatus::infeasible;
            out.message = "minlp: master MILP infeasible (no integer point)";
            return finalize();
        }

        // The OA master is a relaxation. Its unboundedness does not imply
        // that the original convex MINLP is unbounded: nonlinear convex
        // constraints can bound the feasible region even when their current
        // supporting hyperplanes do not. Do not propagate that status and
        // never invent a finite epigraph bound (contract §7.3).
        if (master_sol.status == lp::reference::SolveStatus::unbounded) {
            out.status = lp::reference::SolveStatus::iteration_limit;
            out.message = "minlp: OA master relaxation is unbounded; original MINLP "
                          "boundedness and global optimality are undetermined";
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }

        // 5) Lower bound only from the master assurance layer (contract §7.1).
        if (certified_master_status(master_sol.status) &&
            std::isfinite(master_sol.best_bound)) {
            best_bound = std::max(best_bound, master_sol.best_bound);
            out.best_bound = best_bound;
            out.bound_provenance = "milp_master_certified";
        } else if (out.integer_feasible) {
            out.status = lp::reference::SolveStatus::feasible;
            out.message = "minlp: OA master returned no certified bound; verified "
                          "incumbent retained, global result inconclusive: " +
                          master_sol.message;
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        } else {
            out.status = master_sol.status;
            out.message = "minlp: OA master did not return a certified bound: " +
                          master_sol.message;
            return finalize();
        }

        // 6) Gap check (LOCKED 1e-3 relative).
        if (out.integer_feasible) {
            const double bound_tolerance = options.feasibility_tolerance *
                                           std::max(1.0, std::abs(best_obj));
            if (best_bound > best_obj + bound_tolerance) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: master lower bound exceeds the feasible incumbent; "
                              "global optimality cannot be certified";
                retain_incumbent(out, best_x, best_obj, best_bound);
                return finalize();
            }
            const double gap = std::max(0.0, best_obj - best_bound) /
                               std::max(1.0, std::abs(best_obj));
            out.relative_gap = gap;
            if (gap <= options.gap_tolerance) {
                out.status = lp::reference::SolveStatus::optimal;
                out.message = "minlp: outer approximation gap satisfied";
                retain_incumbent(out, best_x, best_obj, best_bound);
                return finalize();
            }
        }

        // 7) Next NLP start = master integer solution.
        if (master_sol.primal.size() != nlp.n_vars + 1) {
            out.status = master_sol.status == lp::reference::SolveStatus::resource_limit
                             ? lp::reference::SolveStatus::resource_limit
                             : lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: OA master did not return a complete integer assignment";
            retain_incumbent(out, best_x, best_obj, best_bound);
            return finalize();
        }
        for (std::size_t idx : problem.integer_indices) {
            if (!std::isfinite(master_sol.primal[idx]) ||
                std::abs(master_sol.primal[idx] - std::round(master_sol.primal[idx])) >
                    options.feasibility_tolerance) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: OA master returned a non-integral variable assignment";
                retain_incumbent(out, best_x, best_obj, best_bound);
                return finalize();
            }
        }
        x.assign(master_sol.primal.begin(),
                 master_sol.primal.begin() + static_cast<std::ptrdiff_t>(nlp.n_vars));
    }

    if (out.integer_feasible) {
        out.status = lp::reference::SolveStatus::iteration_limit;
        out.message = "minlp: iteration limit reached before the optimality gap tolerance; "
                      "feasible incumbent returned without an optimality claim";
        retain_incumbent(out, best_x, best_obj, best_bound);
    } else {
        out.status = lp::reference::SolveStatus::iteration_limit;
        out.message = "minlp: maximum outer iterations reached";
    }
    return finalize();
}
}
