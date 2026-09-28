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
    out["proof_message"] = res.proof_message;
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
            key != "presolve" && key != "scale")
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
    return options;
}


py::dict run_solve(const model::Model& model, const api::SolveOptions& options) { return to_python(api::solve_model(model, options)); }

} // namespace bindings
