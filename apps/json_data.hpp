#pragma once

// QP-01 (source-limit split): JsonOutputData outgrew apps/json_output.hpp's
// 300-line cap together with the emitter. The payload struct lives here;
// json_output.hpp includes this header and owns formatting/serialization.
#include "markov_cero/api/solve.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace markov_cero::apps {

struct JsonOutputData {
    std::string certificate_type;
    std::string guarantee_tier, assurance, proof_status, proof_budget_kind, proof_model_fingerprint;
    bool proof_budget_exhausted = false; double proof_budget_time_ms = 0.0;
    std::size_t proof_nodes_used = 0, proof_checked_nodes = 0, proof_witness_values_used = 0, proof_checked_witness_values = 0;
    std::uint32_t proof_format_version = 0; std::uint64_t model_fingerprint = 0;
    std::shared_ptr<const markov_cero::verify::MipProof> mip_proof;
    std::string proof_message;
    double mip_proof_build_ms = 0.0;
    double mip_proof_verify_ms = 0.0;
    std::vector<std::string> variable_names;
    std::vector<std::string> row_names;
    std::vector<double> row_activities;
    std::vector<double> row_lower_slacks;
    std::vector<double> row_upper_slacks;
    std::vector<double> row_duals;
    std::vector<double> reduced_costs;

    std::string resolved_engine;
    std::string stop_reason; std::size_t memory_charged_peak_bytes = 0;
    markov_cero::lp::reference::Result result;
    std::size_t model_rows = 0;
    std::size_t model_cols = 0;
    std::size_t model_nnz = 0;
    bool verified = false;
    double original_objective = 0.0;
    std::vector<double> original_primal;
    bool canonical_verified = false;
    bool original_verified = false;
    std::string original_message;
    bool used_warm_start = false;
    bool used_cold_fallback = false;
    markov_cero::verify::PrimalVerificationReport primal_report;
    markov_cero::verify::ReferenceVerification canonical_report;
    double elapsed_ms = 0.0;
    std::size_t nodes_explored = 0;
    std::size_t total_lp_iterations = 0;
    double best_bound = 0.0;
    double relative_gap = 0.0;
    std::size_t cuts_generated = 0;
    std::size_t heuristics_found = 0;
    double pdlp_tolerance = 0.0;
    double pdlp_res_primal_infeas = 0.0;
    double pdlp_res_dual_infeas = 0.0;
    double pdlp_res_gap = 0.0;
    std::string backend_name;
    // QP-01 contract §5: request (backend_name), threshold recommendation
    // (recommended_backend) and executed path (backend_actually_used).
    std::string recommended_backend;
    std::string backend_actually_used;
    double pdlp_h2d_ms = 0.0;
    double pdlp_kernel_ms = 0.0;
    double pdlp_d2h_ms = 0.0;
    double pdlp_total_ms = 0.0;
    std::string error;
    std::string output_path;
    std::string problem_class;
    std::string classification_reason;
    markov_cero::api::NumericalDiagnostic diagnostic;
    // D-15: stagnation / crossover note (empty when the solve was clean).
    std::string convergence_note;
    // D-16: ADMM rho penalty updates (= KKT re-factorizations) for QP solves.
    std::size_t admm_rho_updates = 0;
    bool ml_requested = false;
    bool ml_model_loaded = false;
    std::size_t ml_scoring_calls = 0;
    std::size_t ml_candidates_scored = 0;
    std::size_t ml_fallback_nodes = 0;
    std::size_t ml_maximum_candidate_count = 0;
    std::string ml_fallback_reason;
};

} // namespace markov_cero::apps
