#include "api_internal.hpp"

namespace markov_cero::api::detail {
void run_milp(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result) {
        auto milp_options = options.milp_options;
        if (options.lp_options.deadline && (!milp_options.deadline ||
            *options.lp_options.deadline < *milp_options.deadline))
            milp_options.deadline = options.lp_options.deadline;
        const auto milp_res = milp::solve(model, milp_options);
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

        out.certificate_type = "incumbent_feasibility; solver_trusted_tree";
        if (result.status == lp::reference::SolveStatus::optimal || !milp_res.primal.empty()) {
            out.original_primal = milp_res.primal;
            out.original_objective = milp_res.objective;
            result.primal = milp_res.primal;
            result.objective = milp_res.objective;

            verify::Candidate candidate{out.original_primal, out.original_objective};
            out.primal_report = verify::verify_primal(model, candidate);
            out.original_verified = out.primal_report.passed;
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

    certify_mip(model, options, out, result);

}

} // namespace markov_cero::api::detail
