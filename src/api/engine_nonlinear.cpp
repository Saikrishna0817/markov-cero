#include "api_internal.hpp"

namespace markov_cero::api::detail {
void run_nonlinear(const model::Model& model, const SolveOptions& options, SolveResult& out,
                   lp::reference::Result& result, core::SolveContext& ctx) {
        io::NlobjBridge bridge{model};
        const auto nlp_model = [&] {
            core::StageScope stage(ctx, "model_build");
            return bridge.build();
        }();
        if (stop_after_deadline(ctx, options, out, result, "NLP model construction")) return;
        // Deterministic start: midpoint of the variable bounds (0 where free).
        std::vector<double> x0(nlp_model.n_vars, 0.0);
        for (std::size_t j = 0; j < nlp_model.n_vars; ++j) {
            const double lo = nlp_model.bound_lower(j);
            const double hi = nlp_model.bound_upper(j);
            if (std::isfinite(lo) && std::isfinite(hi)) {
                x0[j] = 0.5 * (lo + hi);
            }
        }
        if (out.resolved_engine == "sqp") {
            nlp::SqpOptions sqp_opts;
            sqp_opts.deadline = options.lp_options.deadline;
            const auto sol = [&] {
                core::StageScope stage(ctx, "solve");
                auto solved = nlp::solve_sqp(nlp_model, x0, sqp_opts);
                stage.set_count(solved.iterations);
                return solved;
            }();
            out.lp_iterations = sol.iterations;
            out.diagnostic.primal_residual = sol.constraint_violation;
            out.diagnostic.dual_residual = sol.kkt_residual;
            // SQP factorizes nothing per solve; 0.0 = not estimated.
            out.diagnostic.condition_estimate = 0.0;
            result.status = sol.status;
            result.message = sol.message;
            if (stop_after_deadline(ctx, options, out, result, "SQP solve")) return;
            core::StageScope verify_stage(ctx, "verify");
            if (sol.status == lp::reference::SolveStatus::optimal) {
                // C4: independent KKT verification before reporting optimal.
                const auto rep = nlp::verify_nlp_solution(nlp_model, sol, 1e-6);
                out.diagnostic.nlp_stationarity_residual = rep.stationarity_residual;
                out.diagnostic.nlp_inequality_violation = rep.inequality_violation;
                out.diagnostic.nlp_equality_violation = rep.equality_violation;
                out.diagnostic.nlp_worst_dual_sign = rep.worst_dual_sign;
                out.diagnostic.nlp_complementarity_residual = rep.complementarity_residual;
                if (rep.accepted) {
                    result.status = lp::reference::SolveStatus::local_optimal;
                    out.certificate_type = "local_kkt";
                    result.primal = sol.x;
                    result.objective = sol.objective;
                    out.original_primal = sol.x;
                    out.original_objective = sol.objective;
                    out.original_verified = true;
                    out.canonical_verified = true;
                    out.original_message = rep.message;
                } else {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "nlp KKT verification failed: " + rep.message;
                    out.original_message = rep.message;
                    out.diagnostic.failure_site = "nlp_kkt_verification";
                    out.diagnostic.suggested_recovery = "relax_tolerance_or_improve_x0";
                }
            } else {
                out.canonical_verified = false;
                out.original_message = "original primal not applicable";
                out.diagnostic.failure_site = "sqp_solve";
                out.diagnostic.suggested_recovery = "improve_x0_or_relax_kkt_tolerance";
            }
        } else {
            minlp::MinlpProblem problem;
            problem.nlp = nlp_model;
            problem.source_model = model;
            for (std::size_t j = 0; j < nlp_model.n_vars; ++j) {
                if (model.variable_type[j] != model::VariableType::continuous) {
                    problem.integer_indices.push_back(j);
                }
            }
            minlp::MinlpOptions minlp_opts;
            minlp_opts.deadline = options.lp_options.deadline;
            minlp_opts.sqp_options.deadline = options.lp_options.deadline;
            const auto sol = [&] {
                core::StageScope stage(ctx, "solve");
                auto solved = minlp::solve_minlp(problem, x0, minlp_opts);
                stage.set_count(solved.iterations);
                return solved;
            }();
            out.lp_iterations = sol.iterations;
            out.cuts_generated = sol.cuts_added;
            const double sense_sign = model.objective_sense == model::ObjectiveSense::maximize
                                          ? -1.0
                                          : 1.0;
            out.best_bound = sense_sign * sol.best_bound;
            out.relative_gap = sol.relative_gap;
            result.status = sol.status;
            result.message = sol.message;
            if (stop_after_deadline(ctx, options, out, result, "MINLP solve")) return;
            core::StageScope verify_stage(ctx, "verify");
            if (sol.integer_feasible && sol.x.size() == nlp_model.n_vars) {
                const auto feasibility = nlp::verify_nlp_feasibility(
                    nlp_model, sol.x, minlp_opts.feasibility_tolerance);
                bool integral = true;
                for (std::size_t j = 0; j < model.variable_type.size(); ++j) {
                    if (model.variable_type[j] != model::VariableType::continuous &&
                        std::abs(sol.x[j] - std::round(sol.x[j])) >
                            minlp_opts.feasibility_tolerance) {
                        integral = false;
                        break;
                    }
                }
                const double normalized_objective = nlp_model.eval_objective(sol.x);
                const bool objective_consistent =
                    std::isfinite(normalized_objective) && std::isfinite(sol.objective) &&
                    std::abs(normalized_objective - sol.objective) <=
                        minlp_opts.feasibility_tolerance *
                            std::max(1.0, std::abs(normalized_objective));
                out.diagnostic.primal_residual = feasibility.maximum_violation;
                result.primal = sol.x;
                result.objective = sense_sign * normalized_objective;
                out.original_primal = sol.x;
                out.original_objective = result.objective;
                out.original_verified = feasibility.feasible && integral && objective_consistent;
                const bool solver_bound_available =
                    sol.status == lp::reference::SolveStatus::optimal &&
                    std::isfinite(sol.best_bound) && std::isfinite(sol.relative_gap) &&
                    sol.relative_gap <= minlp_opts.gap_tolerance;
                out.canonical_verified = false;
                out.certificate_type = out.original_verified
                    ? "incumbent_feasibility; solver_trusted_oa" : "none";
                if (!out.original_verified) {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "minlp: independent incumbent verification failed (" +
                                     feasibility.message + (integral ? "" : "; non-integral") +
                                     (objective_consistent ? "" : "; objective mismatch") + ")";
                    out.original_message = result.message;
                    out.diagnostic.failure_site = "minlp_incumbent_verification";
                    out.diagnostic.suggested_recovery = "inspect_minlp_subproblem_feasibility";
                } else if (sol.status == lp::reference::SolveStatus::optimal &&
                           !solver_bound_available) {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "minlp: solver reported optimal without a finite, "
                                     "within-tolerance global bound";
                    out.original_message = result.message;
                    out.diagnostic.failure_site = "minlp_global_bound_verification";
                    out.diagnostic.suggested_recovery = "inspect_oa_master_bound_and_gap";
                } else {
                    out.original_message = solver_bound_available
                                               ? "MINLP incumbent feasibility verified; "
                                                 "OA global bound is solver-trusted, not independently replayed"
                                               : "MINLP incumbent feasibility verified; "
                                                 "global optimality not established";
                }
            } else {
                out.original_verified = false;
                out.canonical_verified = false;
                out.original_message = "no feasible MINLP incumbent returned; no primal verification applies";
                if (sol.status == lp::reference::SolveStatus::optimal) {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "minlp: optimal status without a feasible incumbent";
                    out.diagnostic.failure_site = "minlp_incumbent_missing";
                    out.diagnostic.suggested_recovery = "inspect_oa_incumbent_updates";
                } else {
                    out.diagnostic.failure_site = "minlp_solve";
                    out.diagnostic.suggested_recovery = "inspect_cut_accumulation_and_x0";
                }
            }
        }
        // RES-01: SQP/MINLP verification runs with no deadline of its own;
        // one shared-context poll before leaving the engine covers both
        // branches (contract section 4).
        if (stop_after_deadline(ctx, options, out, result, "nonlinear verification")) return;
        out.nodes_explored = 1;
        return;

}

} // namespace markov_cero::api::detail
