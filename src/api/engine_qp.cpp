#include "api_internal.hpp"

namespace markov_cero::api::detail {
void run_qp(const model::Model& model, const SolveOptions& options, SolveResult& out,
            lp::reference::Result& result, core::SolveContext& ctx) {
        const auto qp_model = [&] {
            core::StageScope stage(ctx, "model_build");
            return qp::make_quadratic_model(model);
        }();
        if (stop_after_deadline(ctx, options, out, result, "QP model construction")) return;
        qp::QpOptions qopts;
        qopts.absolute_tolerance = 1e-6;
        qopts.relative_tolerance = 1e-6;
        qopts.max_iterations = (options.lp_options.iteration_limit > 0)
                                   ? options.lp_options.iteration_limit
                                   : 10000;
        qopts.deadline = options.lp_options.deadline;
        qopts.time_limit_seconds = options.lp_options.time_limit_seconds;
        // W3/D-08: GPU ADMM path requires explicit --backend gpu; the threshold
        // and device checks are enforced inside the solver (silent CPU fallback).
        const bool gpu_requested = options.backend == "gpu";
        if (gpu_requested) {
            out.recommended_backend = "gpu";
        }
        qp::AdmmQpSolver local_session;
        auto& repeated_qp_session = options.repeated_qp_session
            ? *options.repeated_qp_session : local_session;
        const auto qpres = [&] {
            core::StageScope stage(ctx, "solve");
            auto solved = qp::solve_qp(qp_model, qopts, repeated_qp_session, gpu_requested);
            stage.set_count(solved.iterations);
            return solved;
        }();
        if (stop_after_deadline(ctx, options, out, result, "QP solve")) return;
        core::StageScope verify_stage(ctx, "verify");
        out.lp_iterations = qpres.iterations;
        out.nodes_explored = 1;
        // D-16: rho changes (each one re-factorized the KKT matrix).
        out.admm_rho_updates = qpres.refactorization_count;
        out.diagnostic.primal_residual = qpres.primal_residual;
        out.diagnostic.dual_residual = qpres.dual_residual;
        out.diagnostic.condition_estimate = qpres.condition_estimate;
        if (qpres.status == qp::QpStatus::optimal) {
            out.diagnostic.failure_site = "none";
            out.diagnostic.suggested_recovery = "none";
            result.status = lp::reference::SolveStatus::optimal;
            result.primal = qpres.x;
            result.objective = qpres.objective_value;
            result.message = "QP solved to optimality";
            out.original_primal = qpres.x;
            out.original_objective = qpres.objective_value;
            const auto rep = qp::verify_qp_solution(qp_model, qpres, 1e-4);
            out.original_verified = rep.passed;
            out.certificate_type = "convex_qp_kkt";
            if (rep.passed) {
                const double sign = model.objective_sense == model::ObjectiveSense::maximize ? -1.0 : 1.0;
                out.row_duals.resize(model.matrix.row_count);
                for (std::size_t i = 0; i < out.row_duals.size(); ++i) out.row_duals[i] = -sign * qpres.y[i];
                out.reduced_costs.resize(model.matrix.column_count);
                for (std::size_t j = 0; j < out.reduced_costs.size(); ++j)
                    out.reduced_costs[j] = -sign * qpres.y[model.matrix.row_count + j];
            }
            out.canonical_verified = rep.passed;
            out.original_message = rep.passed ? "QP KKT certificate verified"
                                              : ("QP verification failed: " + rep.failure_reason);
            if (!rep.passed) {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = out.original_message;
                out.diagnostic.failure_site = "qp_kkt_verification";
                out.diagnostic.suggested_recovery = "tighten_qp_tolerance_or_regularize";
            }
        } else if (qpres.status == qp::QpStatus::primal_infeasible) {
            out.canonical_verified = qp::verify_qp_infeasibility(qp_model, qpres);
            result.status = out.canonical_verified ? lp::reference::SolveStatus::infeasible : lp::reference::SolveStatus::numerical_failure;
            result.certificate = qpres.infeasibility_certificate;
            result.message = out.canonical_verified ? "QP Farkas certificate verified" : "QP infeasibility witness rejected";
            out.original_message = "original primal not applicable";
            out.diagnostic.failure_site = "qp_primal_infeasibility_certificate";
            out.diagnostic.suggested_recovery = "relax_incompatible_row_or_variable_bounds";
        } else if (qpres.status == qp::QpStatus::dual_infeasible) {
            out.canonical_verified = qp::verify_qp_unbounded(qp_model, qpres);
            result.status = out.canonical_verified ? lp::reference::SolveStatus::unbounded : lp::reference::SolveStatus::numerical_failure;
            result.ray = qpres.unbounded_ray;
            result.message = out.canonical_verified ? "QP feasible anchor and recession ray verified" : "QP unboundedness witness rejected";
            out.original_message = "original primal not applicable";
            out.diagnostic.failure_site = "qp_dual_infeasibility_certificate";
            out.diagnostic.suggested_recovery = "add_missing_variable_bounds_or_regularize_cost";
        } else if (qpres.status == qp::QpStatus::non_convex) {
            result.status = lp::reference::SolveStatus::invalid_model;
            result.message = "Non-convex QP objective is not supported: " + qpres.message;
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_convexity_check";
            out.diagnostic.suggested_recovery = "ensure_quadratic_objective_matrix_P_is_positive_semidefinite";
        } else if (qpres.status == qp::QpStatus::unsupported) {
            result.status = lp::reference::SolveStatus::unsupported;
            result.message = "QP unsupported: " + qpres.message;
            out.original_message = result.message;
            const bool kkt_fill_limit = qpres.message == "sparse QP KKT factor exceeds fill limit";
            out.diagnostic.failure_site = kkt_fill_limit ? "qp_kkt_factor_fill_limit"
                                                         : "qp_convexity_check_indeterminate";
            out.diagnostic.suggested_recovery = kkt_fill_limit
                ? "use_a_sparser_formulation_or_an_alternative_qp_method"
                : "rescale_or_provide_a_numerically_certifiable_convex_model";
        } else if (qpres.status == qp::QpStatus::iteration_limit) {
            result.status = lp::reference::SolveStatus::iteration_limit;
            result.message = std::string("QP solve failed: ") + qp::to_string(qpres.status);
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_admm_iteration_limit";
            out.diagnostic.suggested_recovery = "increase_iteration_limit_or_tune_rho_init";
        } else if (qpres.status == qp::QpStatus::time_limit) {
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = "QP wall-clock deadline reached";
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_wall_clock_deadline";
            out.diagnostic.suggested_recovery = "increase_time_limit_or_use_a_warm_start";
        } else {
            result.status = lp::reference::SolveStatus::numerical_failure;
            result.message = std::string("QP solve failed: ") + qp::to_string(qpres.status);
            out.original_message = result.message;
            out.diagnostic.failure_site = "qp_kkt_factorization";
            out.diagnostic.suggested_recovery = "increase_regularization_sigma_or_rescale_problem";
        }

}

} // namespace markov_cero::api::detail
