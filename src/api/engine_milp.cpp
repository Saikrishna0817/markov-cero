#include "api_internal.hpp"

namespace markov_cero::api::detail {
void run_milp(const model::Model& model, const SolveOptions& options, SolveResult& out,
              lp::reference::Result& result, core::SolveContext& ctx) {
        auto milp_options = options.milp_options;
        if (options.lp_options.deadline && (!milp_options.deadline ||
            *options.lp_options.deadline < *milp_options.deadline))
            milp_options.deadline = options.lp_options.deadline;
        // RES-01: the serial search polls the shared context at its phase and
        // node boundaries (resource contract section 4).
        milp_options.context = &ctx;
        const auto search_started = Clock::now();
        const auto milp_res = [&] {
            core::StageScope stage(ctx, "search");
            auto solved = milp::solve(model, milp_options);
            stage.set_count(solved.nodes_explored);
            return solved;
        }();
        result.status = milp_res.status;
        result.message = milp_res.message;
        out.nodes_explored = milp_res.nodes_explored;
        out.lp_iterations = milp_res.lp_iterations;
        out.best_bound = milp_res.best_bound;
        out.relative_gap = milp_res.relative_gap;
        out.cuts_generated = milp_res.cuts_generated;
        out.heuristics_found = milp_res.heuristics_found;
        out.diagnostic.condition_estimate = milp_res.condition_estimate;
        if (milp_res.status == lp::reference::SolveStatus::unsupported) {
            out.diagnostic.failure_site = "milp_node_lp_capacity";
            out.diagnostic.suggested_recovery =
                "inspect_node_relaxation_limits_or_use_a_sparser_formulation";
        }
        out.ml_requested = milp_res.ml_requested;
        out.ml_model_loaded = milp_res.ml_model_loaded;
        out.ml_scoring_calls = milp_res.ml_telemetry.scored_nodes;
        out.ml_candidates_scored = milp_res.ml_telemetry.candidates_scored;
        out.ml_fallback_nodes = milp_res.ml_telemetry.fallback_nodes;
        out.ml_maximum_candidate_count =
            milp_res.ml_telemetry.maximum_candidate_count;
        out.ml_fallback_reason = milp_res.ml_fallback_reason;
        if (stop_after_deadline(ctx, options, out, result, "MILP search")) return;
        // R3 (contract section 3): the serial search runs engine-locally and
        // reports its own stops, so attribute them once the shared context is
        // back at the API layer. The node quota is definitive (the loop exits
        // on that condition); the engine-local time limit is the loop's own
        // deadline derived from the same option. Anything else (queued-node
        // capacity, uncertified node LPs) stays with the honest fallback in
        // apply_resource_stop rather than a guessed reason.
        if (result.status == lp::reference::SolveStatus::resource_limit &&
            ctx.stop_reason() == core::StopReason::none) {
            if (milp_res.nodes_explored >= milp_options.max_nodes) {
                (void)ctx.note_stop(core::StopReason::quota_exhausted);
            } else if (std::isfinite(options.milp_options.time_limit_seconds) &&
                       options.milp_options.time_limit_seconds > 0.0 &&
                       std::chrono::duration<double>(Clock::now() - search_started).count() >=
                           options.milp_options.time_limit_seconds) {
                (void)ctx.note_stop(core::StopReason::deadline_exceeded);
            }
        }

        out.certificate_type = "none";
        {
            core::StageScope verify_stage(ctx, "verify");
            if (result.status == lp::reference::SolveStatus::optimal || !milp_res.primal.empty()) {
                out.original_primal = milp_res.primal;
                out.original_objective = milp_res.objective;
                result.primal = milp_res.primal;
                result.objective = milp_res.objective;

                verify::Candidate candidate{out.original_primal, out.original_objective};
                out.primal_report = verify::verify_primal(model, candidate, {},
                                                          {}, 1e-6, true, ctx.deadline());
                out.original_verified = out.primal_report.passed;
                if (out.original_verified) out.certificate_type = "incumbent_feasibility";
                out.canonical_verified = false;
                const auto viol_desc = format_violation(out.primal_report);
                out.original_message = out.original_verified
                                           ? "original primal verified"
                                           : ("original primal rejected: " + viol_desc);
                if (!out.original_verified) {
                    result.status = lp::reference::SolveStatus::numerical_failure;
                    result.message = "original-model verification failed: " + viol_desc;
                }
            } else if (result.status == lp::reference::SolveStatus::infeasible ||
                       result.status == lp::reference::SolveStatus::unbounded) {
                out.canonical_verified = false;
                out.original_message = "original primal not applicable";
            } else {
                out.original_message = "original primal not applicable";
            }
        }

        if (stop_after_deadline(ctx, options, out, result, "MILP verification")) return;
        {
            core::StageScope certify_stage(ctx, "certify");
            certify_mip(model, options, out, result, ctx, milp_res.obligations);
        }
        if ((result.status == lp::reference::SolveStatus::optimal ||
             result.status == lp::reference::SolveStatus::gap_satisfied) &&
            !out.canonical_verified) {
            result.status = out.original_verified ? lp::reference::SolveStatus::feasible
                                                  : lp::reference::SolveStatus::resource_limit;
            result.message = "independent proof " + out.proof_status +
                             "; optimality not certified";
        }
}

} // namespace markov_cero::api::detail
