#include "cli_options.hpp"
#include "json_output.hpp"
#include "markov_cero/api/solve.hpp"
#include "markov_cero/foundation/build_info.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"

#include <iostream>
#include <string>

namespace {

markov_cero::api::SolveOptions to_solve_options(const markov_cero::apps::CliOptions& cli) {
    markov_cero::api::SolveOptions options;
    options.engine = cli.engine_name;
    options.num_threads = cli.num_threads;
    options.threads_explicit = cli.threads_explicit;
    options.warm_start_path = cli.warm_start_path;
    options.save_basis_path = cli.save_basis_path;
    options.enable_presolve = cli.enable_presolve;
    options.enable_scale = cli.enable_scale;
    options.max_presolve_passes = cli.max_presolve_passes;
    options.ruiz_iterations = cli.ruiz_iterations;
    options.pdlp_tolerance = cli.pdlp_tolerance;
    options.backend = cli.backend_name;
    options.maximum_input_bytes = cli.maximum_input_bytes;
    options.mip_proof_time_limit_seconds = cli.mip_proof_time_limit_seconds;
    options.mip_proof_max_nodes = cli.mip_proof_max_nodes;
    options.mip_proof_max_witness_values = cli.mip_proof_max_witness_values;
    options.lp_options = cli.options;
    options.lp_options.time_limit_seconds = cli.time_limit_seconds;
    options.milp_options = cli.milp_options;
    if (const auto* path = std::getenv("MARKOV_CERO_ML_MODEL")) options.milp_options.ml_model_path = path;
    if (const auto* path = std::getenv("MARKOV_CERO_SB_LOG")) options.milp_options.strong_branching_log_path = path;
    return options;
}

markov_cero::apps::JsonOutputData to_json_data(const markov_cero::api::SolveResult& res,
                                              const markov_cero::api::SolveOptions& options) {
    markov_cero::apps::JsonOutputData data;
    data.certificate_type = res.certificate_type;
    data.mip_proof = res.mip_proof;
    data.proof_message = res.proof_message;
    data.mip_proof_build_ms = res.mip_proof_build_ms;
    data.mip_proof_verify_ms = res.mip_proof_verify_ms;
    data.variable_names = res.variable_names;
    data.row_names = res.row_names;
    data.row_activities = res.row_activities;
    data.row_lower_slacks = res.row_lower_slacks;
    data.row_upper_slacks = res.row_upper_slacks;
    data.row_duals = res.row_duals;
    data.reduced_costs = res.reduced_costs;
    data.resolved_engine = res.resolved_engine;
    data.result.status = res.status;
    data.result.message = res.message;
    data.result.primal = res.primal;
    data.result.objective = res.objective;
    data.result.phase_one_iterations = res.phase_one_iterations;
    data.result.phase_two_iterations = res.phase_two_iterations;
    data.model_rows = res.model_rows;
    data.model_cols = res.model_cols;
    data.model_nnz = res.model_nnz;
    data.verified = res.verified;
    data.original_objective = res.original_objective;
    data.original_primal = res.original_primal;
    data.canonical_verified = res.canonical_verified;
    data.original_verified = res.original_verified;
    data.original_message = res.original_message;
    data.used_warm_start = res.used_warm_start;
    data.used_cold_fallback = res.used_cold_fallback;
    data.primal_report = res.primal_report;
    data.canonical_report = res.canonical_report;
    data.elapsed_ms = res.runtime_ms;
    data.nodes_explored = res.nodes_explored;
    data.total_lp_iterations = res.lp_iterations;
    data.best_bound = res.best_bound;
    data.relative_gap = res.relative_gap;
    data.cuts_generated = res.cuts_generated;
    data.heuristics_found = res.heuristics_found;
    data.pdlp_tolerance = options.pdlp_tolerance;
    data.pdlp_res_primal_infeas = res.pdlp_primal_infeasibility;
    data.pdlp_res_dual_infeas = res.pdlp_dual_infeasibility;
    data.pdlp_res_gap = res.pdlp_duality_gap;
    data.backend_name = options.backend;
    data.pdlp_h2d_ms = res.pdlp_h2d_ms;
    data.pdlp_kernel_ms = res.pdlp_kernel_ms;
    data.pdlp_d2h_ms = res.pdlp_d2h_ms;
    data.pdlp_total_ms = res.pdlp_total_ms;
    data.error = res.error;
    data.problem_class = res.problem_class;
    data.classification_reason = res.classification_reason;
    data.recommended_backend = res.recommended_backend;
    data.diagnostic = res.diagnostic;
    data.convergence_note = res.convergence_note;
    data.admm_rho_updates = res.admm_rho_updates;
    data.ml_requested = res.ml_requested;
    data.ml_model_loaded = res.ml_model_loaded;
    data.ml_scoring_calls = res.ml_scoring_calls;
    data.ml_candidates_scored = res.ml_candidates_scored;
    data.ml_fallback_nodes = res.ml_fallback_nodes;
    data.ml_maximum_candidate_count = res.ml_maximum_candidate_count;
    data.ml_fallback_reason = res.ml_fallback_reason;
    return data;
}

} // namespace

int main(int argc, char** argv) {
    using markov_cero::apps::json_number;
    auto cli = markov_cero::apps::CliOptions::parse(argc, argv);
    if (cli.help_requested) return 0;
    if (cli.error) return cli.exit_code;

    const auto options = to_solve_options(cli);
    const auto res = markov_cero::api::solve_file(cli.path, options);
    if (res.input_open_failed) {
        std::cerr << "cannot open input\n";
        return 8;
    }

    auto out_data = to_json_data(res, options);
    out_data.output_path = cli.output_path;
    markov_cero::apps::emit_json_output(out_data);

    std::string timing_diag;
    if (res.resolved_engine == "pdlp") {
        if (options.backend == "gpu") {
            timing_diag = " [gpu H2D=" + json_number(res.pdlp_h2d_ms) + "ms kernel=" +
                          json_number(res.pdlp_kernel_ms) + "ms D2H=" +
                          json_number(res.pdlp_d2h_ms) + "ms total=" +
                          json_number(res.pdlp_total_ms) + "ms]";
        } else {
            timing_diag = " [cpu total=" + json_number(res.pdlp_total_ms) + "ms]";
        }
    }

    std::cerr << "markov-cero " << markov_cero::foundation::version() << " "
              << markov_cero::lp::reference::to_string(res.status)
              << " [" << res.problem_class << " -> " << res.resolved_engine << "]"
              << (res.resolved_engine == "pdlp"
                      ? (" [tol=" + json_number(options.pdlp_tolerance) + "]" + timing_diag)
                      : "")
              << (res.verified ? " VERIFIED\n" : " NOT VERIFIED\n");
    // C-3: a numerical failure must always reach the console with residuals
    // and a suggested action, not just as a bare status word.
    if (res.status == markov_cero::lp::reference::SolveStatus::numerical_failure) {
        const auto& d = res.diagnostic;
        std::cerr << "numerical_diagnostic: primal_residual=" << json_number(d.primal_residual)
                  << " dual_residual=" << json_number(d.dual_residual)
                  << " complementarity_gap=" << json_number(d.complementarity_gap)
                  << " condition_estimate=" << json_number(d.condition_estimate)
                  << " failure_site=" << d.failure_site
                  << " suggested_recovery=" << d.suggested_recovery << "\n";
    }
    if (!res.convergence_note.empty()) {
        std::cerr << "convergence_note: " << res.convergence_note << "\n";
    }
    return markov_cero::apps::exit_code(res.status);
}
