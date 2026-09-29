#include "api_internal.hpp"

namespace markov_cero::api::detail {
void run_pdlp(const model::Model& model, const SolveOptions& options, SolveResult& out,
              lp::reference::Result& result, core::SolveContext& ctx) {
        lp::first_order::PdlpOptions pdlp_opts;
        pdlp_opts.backend = (options.backend == "gpu") ? lp::first_order::Backend::gpu
                                                       : lp::first_order::Backend::cpu;
        pdlp_opts.max_iterations = (options.lp_options.iteration_limit != 10000 &&
                                    options.lp_options.iteration_limit > 0)
                                       ? options.lp_options.iteration_limit
                                       : 100000;
        pdlp_opts.set_tolerance(options.pdlp_tolerance);
        pdlp_opts.enable_crossover = options.enable_pdlp_crossover;
        pdlp_opts.deadline = options.lp_options.deadline;
        const auto pdlp_res = [&] {
            core::StageScope stage(ctx, "solve");
            auto solved = lp::first_order::solve_pdlp(model, pdlp_opts);
            stage.set_count(solved.iterations);
            return solved;
        }();
        if (stop_after_deadline(ctx, options, out, result, "PDLP solve")) return;
        core::StageScope verify_stage(ctx, "verify");
        out.lp_iterations = pdlp_res.iterations;
        out.pdlp_primal_infeasibility = pdlp_res.primal_infeasibility;
        out.pdlp_dual_infeasibility = pdlp_res.dual_infeasibility;
        out.pdlp_duality_gap = pdlp_res.duality_gap;
        out.pdlp_h2d_ms = pdlp_res.h2d_ms;
        out.pdlp_kernel_ms = pdlp_res.kernel_ms;
        out.pdlp_d2h_ms = pdlp_res.d2h_ms;
        out.pdlp_total_ms = pdlp_res.total_ms;
        out.diagnostic.primal_residual = pdlp_res.primal_infeasibility;
        out.diagnostic.dual_residual = pdlp_res.dual_infeasibility;
        // D-15: explicit stagnation/crossover note surfaced in JSON output.
        out.convergence_note = pdlp_res.convergence_note;
        // C-3: |pobj - dobj| of the reported PDLP iterate (the engine's dual
        // objective carries the row- and bound-complementary terms).
        out.diagnostic.complementarity_gap =
            std::abs(pdlp_res.objective - pdlp_res.dual_objective);
        // Matrix-free PDLP has no factorization; only the crossover basis
        // (when one was certified) contributes an estimate.
        out.diagnostic.condition_estimate = pdlp_res.condition_estimate;
        if (pdlp_res.status == lp::first_order::PdlpStatus::optimal) {
            out.diagnostic.failure_site = "none";
            out.diagnostic.suggested_recovery = "none";
            result.status = lp::reference::SolveStatus::optimal;
            result.primal = pdlp_res.primal;
            result.dual = pdlp_res.dual;
            result.objective = pdlp_res.objective;
            result.message = pdlp_res.message;
            out.original_primal = pdlp_res.primal;
            out.original_objective = pdlp_res.objective;

            verify::Candidate candidate{out.original_primal, out.original_objective};
            const verify::Tolerance pdlp_tol{options.pdlp_tolerance, options.pdlp_tolerance};
            out.primal_report = verify::verify_primal(model, candidate, pdlp_tol, pdlp_tol,
                                                      options.pdlp_tolerance, true,
                                                      ctx.deadline());
            out.original_verified = out.primal_report.passed;
            const auto certificate = verify::verify_linear_solution(model, pdlp_res.primal,
                pdlp_res.dual, pdlp_res.objective, options.pdlp_tolerance, false,
                ctx.deadline());
            out.canonical_verified = certificate.accepted;
            out.certificate_type = "linear_primal_dual_gap";
            out.best_bound = certificate.bound;
            out.relative_gap = certificate.relative_gap;
            const auto viol_desc = format_violation(out.primal_report);
            out.original_message = out.original_verified
                                       ? "original primal verified"
                                       : ("original primal rejected: " + viol_desc);
            if (out.original_verified && !out.canonical_verified) {
                result.status = lp::reference::SolveStatus::feasible;
                result.message = "PDLP primal feasible; optimality uncertified: " + certificate.message;
                out.certificate_type = "primal_feasibility";
            } else if (!out.original_verified) {
                result.status = lp::reference::SolveStatus::numerical_failure;
                result.message = "PDLP certificate rejected: " + certificate.message + "; " + viol_desc;
                out.diagnostic.failure_site = "pdlp_kkt_verification";
                out.diagnostic.suggested_recovery = "tighten_pdlp_tolerance_or_crossover";
            }
        } else if (pdlp_res.status == lp::first_order::PdlpStatus::iteration_limit) {
            result.status = lp::reference::SolveStatus::iteration_limit;
            result.message = pdlp_res.message;
            out.diagnostic.failure_site = "pdlp_iteration_limit";
            out.diagnostic.suggested_recovery = "crossover_to_dual_simplex_or_increase_iterations";
        } else if (pdlp_res.status == lp::first_order::PdlpStatus::resource_limit) {
            (void)ctx.note_stop(core::StopReason::deadline_exceeded);
            result.status = lp::reference::SolveStatus::resource_limit;
            result.message = pdlp_res.message;
            out.diagnostic.failure_site = "pdlp_wall_clock_deadline";
            out.diagnostic.suggested_recovery = "increase_time_limit_or_use_a_warm_start";
        } else {
            result.status = lp::reference::SolveStatus::numerical_failure;
            result.message = pdlp_res.message;
            out.diagnostic.failure_site = "pdlp_first_order_solver";
            out.diagnostic.suggested_recovery = "crossover_to_dual_simplex_or_tighten_step_sizes";
        }
        out.nodes_explored = 1;

}

} // namespace markov_cero::api::detail
