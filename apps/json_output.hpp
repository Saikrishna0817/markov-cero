#pragma once

#include "markov_cero/api/solve.hpp"
#include "json_data.hpp"
#include "markov_cero/foundation/build_info.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/verify/primal_verifier.hpp"
#include "markov_cero/verify/reference_lp_verifier.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace markov_cero::apps {

inline int exit_code(markov_cero::lp::reference::SolveStatus status) {
    using markov_cero::lp::reference::SolveStatus;
    switch (status) {
    case SolveStatus::optimal:
    case SolveStatus::gap_satisfied:
    case SolveStatus::local_optimal:
    case SolveStatus::feasible: return 0;
    case SolveStatus::infeasible: return 1;
    case SolveStatus::unbounded: return 2;
    case SolveStatus::invalid_model: return 3;
    case SolveStatus::invalid_options: return 4;
    case SolveStatus::resource_limit: return 5;
    case SolveStatus::iteration_limit: return 6;
    case SolveStatus::numerical_failure: return 7;
    }
    return 7;
}

inline std::string format_violation(
    const markov_cero::verify::PrimalVerificationReport& report) {
    if (report.violations.empty()) return "";
    const auto& v = report.violations[0];
    return v.category + " idx=" + std::to_string(v.index) +
           " act=" + std::to_string(v.actual) +
           " bnd=" + std::to_string(v.bound) +
           " diff=" + std::to_string(v.magnitude) +
           " allow=" + std::to_string(v.allowance);
}

inline std::string json_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
            out += buf;
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

inline std::string json_number(double value) {
    if (!std::isfinite(value)) {
        return "null";
    }
    std::ostringstream o;
    o.setf(std::ios::fmtflags(0), std::ios::floatfield);
    o.precision(17);
    o << value;
    return o.str();
}

inline std::string json_array(const std::vector<double>& values) {
    std::ostringstream o;
    o << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) o << ',';
        o << json_number(values[i]);
    }
    o << ']';
    return o.str();
}

inline std::string json_array(const std::vector<std::string>& values) {
    std::string result = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) result += ',';
        result += "\"" + json_escape(values[i]) + "\"";
    }
    return result + "]";
}

inline bool emit_json_output(const JsonOutputData& data) {
    std::ostringstream json;
    json << "{\"version\":\"" << json_escape(std::string(markov_cero::foundation::version()))
         << "\","
         << "\"milestone\":\"" << json_escape(std::string(markov_cero::foundation::milestone()))
         << "\","
         << "\"engine\":\"" << json_escape(data.resolved_engine) << "\","
         << "\"problem_class\":\"" << json_escape(data.problem_class) << "\","
         << "\"status\":\"" << markov_cero::lp::reference::to_string(data.result.status) << "\","
         << "\"stop_reason\":\"" << json_escape(data.stop_reason) << "\",\"memory_charged_peak_bytes\":" << data.memory_charged_peak_bytes
         << ",\"device_memory_charged_peak_bytes\":" << data.device_memory_charged_peak_bytes << ","
         << "\"rows\":" << data.model_rows << ","
         << "\"cols\":" << data.model_cols << ","
         << "\"nonzeros\":" << data.model_nnz << ","
         << "\"certificate_type\":\"" << json_escape(data.certificate_type) << "\",\"assurance\":\"" << json_escape(data.assurance) << "\","
         << "\"model_fingerprint\":" << data.model_fingerprint << ",\"guarantee_tier\":\"" << json_escape(data.guarantee_tier) << "\","
         << "\"proof_status\":\"" << json_escape(data.proof_status) << "\",\"proof_budget_exhausted\":" << (data.proof_budget_exhausted ? "true" : "false") << ","
         << "\"proof_budget_kind\":\"" << json_escape(data.proof_budget_kind) << "\",\"proof_nodes_used\":" << data.proof_nodes_used << ",\"proof_checked_nodes\":" << data.proof_checked_nodes << ","
         << "\"proof_witness_values_used\":" << data.proof_witness_values_used << ",\"proof_checked_witness_values\":" << data.proof_checked_witness_values << ",\"proof_budget_time_ms\":" << json_number(data.proof_budget_time_ms) << ","
         << "\"proof_format_version\":" << data.proof_format_version << ",\"proof_model_fingerprint\":\"" << json_escape(data.proof_model_fingerprint) << "\","
         << "\"variable_names\":" << json_array(data.variable_names) << ","
         << "\"row_names\":" << json_array(data.row_names) << ","
         << "\"row_activities\":" << json_array(data.row_activities) << ","
         << "\"row_lower_slacks\":" << json_array(data.row_lower_slacks) << ","
         << "\"row_upper_slacks\":" << json_array(data.row_upper_slacks) << ","
         << "\"row_duals\":" << json_array(data.row_duals) << ","
         << "\"reduced_costs\":" << json_array(data.reduced_costs) << ","
         << "\"verified\":" << (data.verified ? "true" : "false") << ","
         << "\"message\":\"" << json_escape(data.result.message) << "\","
         << "\"objective\":"
         << json_number(data.result.status == markov_cero::lp::reference::SolveStatus::optimal
                            ? data.original_objective
                            : data.result.objective)
         << ","
         << "\"primal\":"
         << json_array(data.original_primal.empty() ? data.result.primal
                                                    : data.original_primal)
         << ","
         << "\"canonical_verified\":" << (data.canonical_verified ? "true" : "false") << ","
         << "\"original_verified\":" << (data.original_verified ? "true" : "false") << ","
         << "\"original_message\":\"" << json_escape(data.original_message) << "\","
         << "\"used_warm_start\":" << (data.used_warm_start ? "true" : "false") << ","
         << "\"used_cold_fallback\":" << (data.used_cold_fallback ? "true" : "false") << ","
         << "\"maximum_primal_violation\":" << json_number(data.primal_report.maximum_row_violation)
         << ","
         << "\"maximum_variable_violation\":"
         << json_number(data.primal_report.maximum_variable_violation) << ","
         << "\"maximum_integrality_violation\":"
         << json_number(data.primal_report.maximum_integrality_violation) << ","
         << "\"maximum_canonical_primal_violation\":"
         << json_number(data.canonical_report.maximum_primal_violation) << ","
         << "\"maximum_canonical_dual_violation\":"
         << json_number(data.canonical_report.maximum_dual_violation) << ","
         << "\"runtime_ms\":" << json_number(data.elapsed_ms) << ","
         << "\"nodes_explored\":" << data.nodes_explored << ","
         << "\"lp_iterations\":" << data.total_lp_iterations << ","
         << "\"best_bound\":" << json_number(data.best_bound) << ","
         << "\"relative_gap\":" << json_number(data.relative_gap) << ","
         << "\"cuts_generated\":" << data.cuts_generated << ","
         << "\"heuristics_found\":" << data.heuristics_found << ","
         << "\"phase_one_iterations\":" << data.result.phase_one_iterations << ","
         << "\"phase_two_iterations\":" << data.result.phase_two_iterations << ","
         << "\"pdlp_tolerance\":" << json_number(data.pdlp_tolerance) << ","
         << "\"relative_primal_residual\":" << json_number(data.pdlp_res_primal_infeas) << ","
         << "\"relative_dual_residual\":" << json_number(data.pdlp_res_dual_infeas) << ","
         << "\"relative_duality_gap\":" << json_number(data.pdlp_res_gap) << ","
         << "\"backend\":\"" << json_escape(data.backend_name) << "\","
         << "\"recommended_backend\":\"" << json_escape(data.recommended_backend) << "\","
         << "\"backend_actually_used\":\"" << json_escape(data.backend_actually_used) << "\","
         << "\"h2d_ms\":" << json_number(data.pdlp_h2d_ms) << ","
         << "\"kernel_ms\":" << json_number(data.pdlp_kernel_ms) << ","
         << "\"d2h_ms\":" << json_number(data.pdlp_d2h_ms) << ","
         << "\"total_ms\":"
         << json_number(data.resolved_engine == "pdlp" ? data.pdlp_total_ms
                                                       : data.elapsed_ms)
         << ","
          << "\"limitations\":\"Sovereign LP/MILP/QP/MIQP (CPU/GPU) engine.\""
          << ",\"diagnostic\":{"
          << "\"primal_residual\":" << json_number(data.diagnostic.primal_residual) << ","
          << "\"dual_residual\":" << json_number(data.diagnostic.dual_residual) << ","
          << "\"complementarity_gap\":"
          << json_number(data.diagnostic.complementarity_gap) << ","
          << "\"nlp_stationarity_residual\":"
          << json_number(data.diagnostic.nlp_stationarity_residual) << ","
          << "\"nlp_inequality_violation\":"
          << json_number(data.diagnostic.nlp_inequality_violation) << ","
          << "\"nlp_equality_violation\":"
          << json_number(data.diagnostic.nlp_equality_violation) << ","
          << "\"nlp_worst_dual_sign\":"
          << json_number(data.diagnostic.nlp_worst_dual_sign) << ","
          << "\"nlp_complementarity_residual\":"
          << json_number(data.diagnostic.nlp_complementarity_residual) << ","
          << "\"condition_estimate\":" << json_number(data.diagnostic.condition_estimate) << ","
          << "\"failure_site\":\"" << json_escape(data.diagnostic.failure_site) << "\","
          << "\"suggested_recovery\":\"" << json_escape(data.diagnostic.suggested_recovery) << "\""
          << "}"
          << ",\"admm_rho_updates\":" << data.admm_rho_updates
          << ",\"ml_requested\":" << (data.ml_requested ? "true" : "false")
          << ",\"ml_model_loaded\":" << (data.ml_model_loaded ? "true" : "false")
          << ",\"ml_scoring_calls\":" << data.ml_scoring_calls
          << ",\"ml_candidates_scored\":" << data.ml_candidates_scored
          << ",\"ml_fallback_nodes\":" << data.ml_fallback_nodes
          << ",\"ml_maximum_candidate_count\":" << data.ml_maximum_candidate_count
          << ",\"ml_fallback_reason\":\""
          << json_escape(data.ml_fallback_reason) << "\"";
    if (!data.convergence_note.empty()) {
        json << ",\"convergence_note\":\"" << json_escape(data.convergence_note) << "\"";
    }
    json << ",\"proof_message\":\"" << json_escape(data.proof_message) << "\""
         << ",\"mip_proof_build_ms\":" << json_number(data.mip_proof_build_ms)
         << ",\"mip_proof_verify_ms\":" << json_number(data.mip_proof_verify_ms);
    if (data.mip_proof) {
        std::ostringstream proof;
        markov_cero::verify::write_mip_proof(proof, *data.mip_proof);
        json << ",\"mip_proof\":\"" << json_escape(proof.str()) << "\"";
    }
    json << ",\"oa_proof_build_ms\":" << json_number(data.oa_proof_build_ms)
         << ",\"oa_proof_verify_ms\":" << json_number(data.oa_proof_verify_ms);
    if (data.oa_proof) {
        std::ostringstream proof;
        markov_cero::verify::write_oa_proof(proof, *data.oa_proof);
        json << ",\"oa_proof\":\"" << json_escape(proof.str()) << "\"";
    }
    if (!data.error.empty()) {
        json << ",\"error\":\"" << json_escape(data.error) << "\"";
    }
    json << "}\n";
    const std::string payload = json.str();
    std::cout << payload;
    if (!data.output_path.empty()) {
        std::ofstream output(data.output_path);
        if (!output || !(output << payload)) {
            std::cerr << "cannot write output\n";
            return false;
        }
    }
    return true;
}

} // namespace markov_cero::apps
