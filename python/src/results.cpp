#include "bindings_internal.hpp"
#include <cstring>
#include <sstream>
#include "markov_cero/verify/mip_proof.hpp"
namespace bindings {
// Shared implementation behind mc.solve and Model.solve: run the API on an
// assembled model::Model and translate the result into a Python dict.
py::dict to_python(api::SolveResult res) {
    py::dict out;
    out["status"] = std::string(lp::reference::to_string(res.status));
    out["message"] = res.message;
    out["engine"] = res.resolved_engine;
    out["problem_class"] = res.problem_class;
    out["classification_reason"] = res.classification_reason;
    out["stop_reason"] = res.stop_reason;
    // W01/D16: stable identity of the validated model that produced this
    // result (mix of structural and numeric content hashes).
    out["model_fingerprint"] = res.model_fingerprint;
    out["objective"] = res.original_objective != 0.0 || !res.original_primal.empty()
                           ? res.original_objective
                           : res.objective;
    // Zero-copy out: move the witness into a float64 array that adopts the
    // vector's storage (no element copy at the boundary).
    std::vector<double> witness;
    if (!res.original_primal.empty()) {
        witness = std::move(res.original_primal);
    } else {
        witness = std::move(res.primal);
    }
    out["x"] = adopt_vector(std::move(witness));
    out["verified"] = res.verified;
    out["original_verified"] = res.original_verified;
    out["canonical_verified"] = res.canonical_verified;
    out["certificate_type"] = res.certificate_type;
    out["guarantee_tier"] = res.guarantee_tier;
    out["proof_status"] = res.proof_status;
    out["proof_budget_exhausted"] = res.proof_budget_exhausted;
    out["proof_budget_kind"] = res.proof_budget_kind;
    out["proof_nodes_used"] = res.proof_nodes_used;
    out["proof_checked_nodes"] = res.proof_checked_nodes;
    out["proof_witness_values_used"] = res.proof_witness_values_used;
    out["proof_checked_witness_values"] = res.proof_checked_witness_values;
    out["proof_budget_time_ms"] = res.proof_budget_time_ms;
    out["proof_format_version"] = res.proof_format_version;
    out["proof_model_fingerprint"] = res.proof_model_fingerprint;
    out["proof_message"] = res.proof_message;
    out["mip_proof_build_ms"] = res.mip_proof_build_ms;
    out["mip_proof_verify_ms"] = res.mip_proof_verify_ms;
    if (res.mip_proof) {
        std::ostringstream proof; verify::write_mip_proof(proof, *res.mip_proof);
        out["mip_proof"] = proof.str();
    } else out["mip_proof"] = py::none();
    out["variable_names"] = res.variable_names;
    out["row_names"] = res.row_names;
    out["row_activities"] = res.row_activities;
    out["row_duals"] = res.row_duals;
    out["reduced_costs"] = res.reduced_costs;
    out["runtime_ms"] = res.runtime_ms;
    out["nodes"] = res.nodes_explored;
    out["lp_iterations"] = res.lp_iterations;
    out["relative_gap"] = res.relative_gap;
    out["best_bound"] = res.best_bound;
    out["cuts"] = res.cuts_generated;
    py::dict diag;
    diag["primal_residual"] = res.diagnostic.primal_residual;
    diag["dual_residual"] = res.diagnostic.dual_residual;
    diag["failure_site"] = res.diagnostic.failure_site;
    diag["suggested_recovery"] = res.diagnostic.suggested_recovery;
    out["diagnostic"] = diag;
    return out;
}

api::SolveOptions options_from_kwargs(const py::kwargs& kwargs) {
    api::SolveOptions options;
    if (kwargs.contains("options")) options = py::cast<api::SolveOptions>(kwargs["options"]);
    for (auto item : kwargs) {
        const auto key = py::cast<std::string>(item.first);
        if (key != "options" && key != "engine" && key != "threads" && key != "backend" &&
            key != "presolve" && key != "scale" && key != "proof_time_limit" &&
            key != "proof_max_nodes" && key != "proof_max_witness_values" &&
            key != "max_queued_nodes" && key != "max_input_bytes" &&
            key != "time_limit" && key != "memory_limit_bytes")
            throw std::invalid_argument("unknown solve option: " + key);
    }
    if (kwargs.contains("engine")) {
        options.engine = py::str(kwargs["engine"]);
    }
    if (kwargs.contains("threads")) {
        options.num_threads = py::int_(kwargs["threads"]);
        options.threads_explicit = true;
    }
    if (kwargs.contains("backend")) {
        options.backend = py::str(kwargs["backend"]);
    }
    if (kwargs.contains("presolve")) {
        options.enable_presolve = py::bool_(kwargs["presolve"]);
    }
    if (kwargs.contains("scale")) {
        options.enable_scale = py::bool_(kwargs["scale"]);
    }
    if (kwargs.contains("proof_time_limit"))
        options.mip_proof_time_limit_seconds = py::cast<double>(kwargs["proof_time_limit"]);
    if (kwargs.contains("proof_max_nodes"))
        options.mip_proof_max_nodes = py::cast<std::size_t>(kwargs["proof_max_nodes"]);
    if (kwargs.contains("proof_max_witness_values"))
        options.mip_proof_max_witness_values = py::cast<std::size_t>(kwargs["proof_max_witness_values"]);
    if (kwargs.contains("max_queued_nodes")) {
        const auto count = py::cast<std::size_t>(kwargs["max_queued_nodes"]);
        if (!count || count > 10000000)
            throw std::invalid_argument("max_queued_nodes must be in 1..10000000");
        options.milp_options.max_queued_nodes = count;
    }
    if (kwargs.contains("max_input_bytes")) {
        const auto count = py::cast<std::size_t>(kwargs["max_input_bytes"]);
        if (!count || count > 1073741824ULL)
            throw std::invalid_argument("max_input_bytes must be in 1..1073741824");
        options.maximum_input_bytes = count;
    }
    if (kwargs.contains("time_limit"))
        options.total_time_limit_seconds = py::cast<double>(kwargs["time_limit"]);
    if (kwargs.contains("memory_limit_bytes"))
        options.memory_limit_bytes = py::cast<std::size_t>(kwargs["memory_limit_bytes"]);
    return options;
}


py::dict run_solve(const model::Model& model, const api::SolveOptions& options) { return to_python(api::solve_model(model, options)); }

} // namespace bindings
