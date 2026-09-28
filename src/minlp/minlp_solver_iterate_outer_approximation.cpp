#include "minlp_solver_internal.hpp"
namespace markov_cero::minlp {
using namespace detail_minlp_solver;
MinlpSolution iterate_outer_approximation(const MinlpProblem& problem, const NlpModel& nlp,
    const std::vector<double>& x0, const MinlpOptions& options) {
    MinlpSolution out;
    double best_obj = kInf;
    std::vector<double> best_x;
    double best_bound = -kInf;
    std::size_t cuts = 0;
    std::size_t sqp_failures = 0;

    // Accumulated master rows.
    std::vector<std::vector<double>> obj_grads;
    std::vector<double> obj_rhs;   // grad f(x^k)^T x^k - f(x^k)
    std::vector<std::vector<double>> cut_grads;
    std::vector<double> cut_rhs;   // J_i(x^k)^T x^k - g_i(x^k)

    // Add objective + constraint rows at point p (works for feasible and
    // infeasible points alike; rows are valid under-approximations for convex
    // problems at ANY point).
    const auto add_rows_at = [&](const std::vector<double>& p, double f_at_p) {
        std::vector<double> grad_f = nlp.eval_gradient(p);
        double dot = 0.0;
        for (std::size_t j = 0; j < nlp.n_vars; ++j) {
            dot += grad_f[j] * p[j];
        }
        obj_grads.push_back(std::move(grad_f));
        obj_rhs.push_back(dot - f_at_p);
        std::size_t n_ineq = 0, n_eq = 0;
        const auto cvals = constraint_values(nlp, p, n_ineq, n_eq);
        const auto J = constraint_jacobian(nlp, p, n_ineq, n_eq);
        for (std::size_t i = 0; i < n_ineq; ++i) {
            double shift = 0.0;
            for (std::size_t j = 0; j < nlp.n_vars; ++j) {
                shift += J[i][j] * p[j];
            }
            cut_grads.push_back(J[i]);
            cut_rhs.push_back(shift - cvals[i]);
            ++cuts;
        }
        out.cuts_added = cuts;
    };

    std::vector<double> x = x0;
    for (std::size_t iter = 0; iter < options.max_iterations; ++iter) {
        if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline) {
            out.status = lp::reference::SolveStatus::resource_limit;
            out.message = "minlp outer-approximation wall-clock deadline reached";
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
                out.best_bound = best_bound;
            }
            return out;
        }
        out.iterations = iter + 1;

        // 1) NLP subproblem. Iteration 1 (and any iteration where the master
        // has no usable integer point): relaxed start. Afterwards the master's
        // integer solution pins the integer variables.
        // The first subproblem is the continuous relaxation. Later subproblems
        // fix the integer variables to the master MILP assignment, as required
        // by the OA loop. Fixing them to x0 on iteration zero would constrain
        // an arbitrary (often fractional) starting guess and is not a valid
        // relaxation step.
        NlpModel fixed_model = iter == 0
                                  ? nlp
                                  : with_fixed_integers(nlp, problem.integer_indices, x);
        const auto sub = nlp::solve_sqp(fixed_model, x, options.sqp_options);
        if (sub.status == lp::reference::SolveStatus::numerical_failure) {
            // OA tolerates failed subproblems: rows at the current point are
            // still valid cuts. Abort only after repeated failures.
            ++sqp_failures;
            if (sqp_failures > 3) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: NLP subproblem failed repeatedly: " + sub.message;
                return out;
            }
        } else {
            sqp_failures = 0;
        }

        const std::vector<double> point =
            sub.x.empty() ? x : sub.x;
        if (point.size() != nlp.n_vars) {
            out.status = lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: NLP subproblem returned a primal with the wrong dimension";
            return out;
        }
        const double point_obj = nlp.eval_objective(point);
        if (!std::isfinite(point_obj)) {
            out.status = lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: NLP subproblem returned a non-finite objective";
            return out;
        }

        // 2) Incumbent update: the fixed-integer subproblem is integer
        // feasible by construction when it converges cleanly.
        bool integer_ok = true;
        for (std::size_t idx : problem.integer_indices) {
            if (std::abs(point[idx] - std::round(point[idx])) > 1e-6) {
                integer_ok = false;
                break;
            }
        }
        const double actual_violation = nlp::constraint_violation(nlp, point);
        const double actual_objective = nlp.eval_objective(point);
        if (integer_ok && sub.status == lp::reference::SolveStatus::optimal &&
            std::isfinite(actual_objective) && std::isfinite(actual_violation) &&
            actual_violation <= options.feasibility_tolerance) {
            if (actual_objective < best_obj) {
                best_obj = actual_objective;
                best_x = point;
                out.integer_feasible = true;
            }
        }

        // 3) Refine the outer approximation at the subproblem point.
        add_rows_at(point, point_obj);

        // 4) Solve the master MILP (existing engine, unmodified per D-03).
        model::Model master = build_master(nlp, problem.integer_indices,
                                           obj_grads, obj_rhs,
                                           cut_grads, cut_rhs);
        try {
            master.validate();
        } catch (const std::exception& e) {
            out.status = lp::reference::SolveStatus::invalid_model;
            out.message = std::string("minlp: master model invalid: ") + e.what();
            return out;
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
                if (out.integer_feasible) {
                    out.x = best_x;
                    out.objective = best_obj;
                    out.best_bound = best_bound;
                }
                return out;
            }
            milp_opts.time_limit_seconds = std::min(milp_opts.time_limit_seconds,
                                                    remaining_seconds);
        }
        milp_opts.enable_cuts = false;   // OA rows are the cuts; keep master lean
        milp_opts.enable_heuristics = false;
        milp_opts.enable_strong_branching = false;
        const auto master_sol = milp::solve(master, milp_opts);

        if (master_sol.status == lp::reference::SolveStatus::infeasible) {
            if (out.integer_feasible) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: master reported infeasible despite a verified incumbent; "
                              "global optimality cannot be certified";
                out.x = best_x;
                out.objective = best_obj;
                return out;
            }
            out.status = lp::reference::SolveStatus::infeasible;
            out.message = "minlp: master MILP infeasible (no integer point)";
            return out;
        }

        // The OA master is a relaxation. Its unboundedness does not imply
        // that the original convex MINLP is unbounded: nonlinear convex
        // constraints can bound the feasible region even when their current
        // supporting hyperplanes do not. Do not propagate that status.
        if (master_sol.status == lp::reference::SolveStatus::unbounded) {
            out.status = lp::reference::SolveStatus::iteration_limit;
            out.message = "minlp: OA master relaxation is unbounded; original MINLP "
                          "boundedness and global optimality are undetermined";
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
            }
            return out;
        }

        // 5) Lower bound from the master (its dual bound stays valid even on
        // an early stop).
        if (master_sol.status == lp::reference::SolveStatus::optimal ||
            master_sol.status == lp::reference::SolveStatus::iteration_limit ||
            master_sol.status == lp::reference::SolveStatus::resource_limit) {
            if (std::isfinite(master_sol.best_bound)) {
                best_bound = std::max(best_bound, master_sol.best_bound);
            }
            out.best_bound = best_bound;
        } else {
            out.status = master_sol.status;
            out.message = "minlp: OA master did not return a certified bound: " +
                          master_sol.message;
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
            }
            return out;
        }

        // 6) Gap check (LOCKED 1e-3 relative).
        if (out.integer_feasible) {
            const double bound_tolerance = options.feasibility_tolerance *
                                           std::max(1.0, std::abs(best_obj));
            if (best_bound > best_obj + bound_tolerance) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: master lower bound exceeds the feasible incumbent; "
                              "global optimality cannot be certified";
                out.x = best_x;
                out.objective = best_obj;
                return out;
            }
            const double gap = std::max(0.0, best_obj - best_bound) /
                               std::max(1.0, std::abs(best_obj));
            out.relative_gap = gap;
            if (gap <= options.gap_tolerance) {
                out.status = lp::reference::SolveStatus::optimal;
                out.message = "minlp: outer approximation gap satisfied";
                out.x = best_x;
                out.objective = best_obj;
                return out;
            }
        }

        // 7) Next NLP start = master integer solution.
        if (master_sol.primal.size() != nlp.n_vars + 1) {
            out.status = master_sol.status == lp::reference::SolveStatus::resource_limit
                             ? lp::reference::SolveStatus::resource_limit
                             : lp::reference::SolveStatus::numerical_failure;
            out.message = "minlp: OA master did not return a complete integer assignment";
            if (out.integer_feasible) {
                out.x = best_x;
                out.objective = best_obj;
            }
            return out;
        }
        for (std::size_t idx : problem.integer_indices) {
            if (!std::isfinite(master_sol.primal[idx]) ||
                std::abs(master_sol.primal[idx] - std::round(master_sol.primal[idx])) >
                    options.feasibility_tolerance) {
                out.status = lp::reference::SolveStatus::numerical_failure;
                out.message = "minlp: OA master returned a non-integral variable assignment";
                if (out.integer_feasible) {
                    out.x = best_x;
                    out.objective = best_obj;
                }
                return out;
            }
        }
        x.assign(master_sol.primal.begin(),
                 master_sol.primal.begin() + static_cast<std::ptrdiff_t>(nlp.n_vars));
    }

    if (out.integer_feasible) {
        out.status = lp::reference::SolveStatus::iteration_limit;
        out.message = "minlp: iteration limit reached before the optimality gap tolerance; "
                      "feasible incumbent returned without an optimality claim";
        out.x = best_x;
        out.objective = best_obj;
        out.best_bound = best_bound;
        out.relative_gap = std::abs(best_obj - best_bound) /
                           std::max(1.0, std::abs(best_obj));
    } else {
        out.status = lp::reference::SolveStatus::iteration_limit;
        out.message = "minlp: maximum outer iterations reached";
    }
    return out;
}
}
