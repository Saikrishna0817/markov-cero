#include "api_internal.hpp"

namespace markov_cero::api::detail {
void run_parallel(const model::Model& model, const SolveOptions& options, SolveResult& out, lp::reference::Result& result) {
        milp::ParallelOptions par_opts;
        par_opts.num_threads = options.num_threads;
        par_opts.time_limit_seconds = options.milp_options.time_limit_seconds;
        par_opts.deadline = options.lp_options.deadline;
        par_opts.max_nodes = options.milp_options.max_nodes;
        par_opts.max_queued_nodes = options.milp_options.max_queued_nodes;
        par_opts.max_iterations = options.milp_options.max_iterations;
        par_opts.relative_gap_tolerance = options.milp_options.relative_gap_tolerance;
        par_opts.absolute_gap_tolerance = options.milp_options.absolute_gap_tolerance;
        par_opts.feasibility_tolerance = options.milp_options.feasibility_tolerance;
        par_opts.integrality_tolerance = options.milp_options.integrality_tolerance;
        par_opts.enable_cuts = options.milp_options.enable_cuts;
        par_opts.enable_mir_cuts = options.milp_options.enable_mir_cuts;
        par_opts.enable_heuristics = options.milp_options.enable_heuristics;
        par_opts.enable_strong_branching = options.milp_options.enable_strong_branching;
        par_opts.branching_strategy = options.milp_options.branching_strategy;
        const auto par_res = milp::solve_parallel(model, par_opts);
        result.status = par_res.status;
        result.message = par_res.message;
        out.nodes_explored = par_res.nodes_explored;
        out.lp_iterations = par_res.lp_iterations;
        out.best_bound = par_res.best_bound;
        out.relative_gap = par_res.relative_gap;
        out.cuts_generated = par_res.cuts_generated;
        out.heuristics_found = par_res.heuristics_found;
        out.diagnostic.condition_estimate = par_res.condition_estimate;

        out.certificate_type = "incumbent_feasibility; solver_trusted_tree";
        if (result.status == lp::reference::SolveStatus::optimal || !par_res.primal.empty()) {
            out.original_primal = par_res.primal;
            out.original_objective = par_res.objective;
            result.primal = par_res.primal;
            result.objective = par_res.objective;

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
