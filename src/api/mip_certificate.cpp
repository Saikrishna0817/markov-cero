#include "api_internal.hpp"
#include "markov_cero/verify/mip_proof.hpp"
namespace markov_cero::api::detail {
void certify_mip(const model::Model& model, const SolveOptions& options, SolveResult& out,
                 lp::reference::Result& result) {
    using Status = lp::reference::SolveStatus;
    if (!options.enable_mip_proof ||
        (result.status != Status::optimal && result.status != Status::gap_satisfied &&
         result.status != Status::infeasible)) return;
    verify::MipProofOptions proof_options;
    proof_options.deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(options.mip_proof_time_limit_seconds));
    proof_options.maximum_nodes = options.mip_proof_max_nodes;
    proof_options.maximum_witness_values = options.mip_proof_max_witness_values;
    if (options.lp_options.deadline && *options.lp_options.deadline < *proof_options.deadline)
        proof_options.deadline = options.lp_options.deadline;
    proof_options.relative_gap = result.status == Status::gap_satisfied
        ? options.milp_options.relative_gap_tolerance : 0;
    auto stage_start = Clock::now();
    bool building = true;
    try {
        auto proof = std::make_shared<verify::MipProof>(verify::build_mip_proof(model,
            out.original_primal, out.original_objective, result.status == Status::infeasible, proof_options));
        const auto verify_start = Clock::now();
        out.mip_proof_build_ms = std::chrono::duration<double, std::milli>(
            verify_start - stage_start).count();
        stage_start = verify_start;
        building = false;
        const auto report = verify::verify_mip_proof(model, *proof, proof_options);
        out.mip_proof_verify_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - verify_start).count();
        out.mip_proof = std::move(proof);
        out.proof_message = report.message;
        out.canonical_verified = report.accepted;
        if (report.accepted) {
            out.certificate_type = "independent_mip_tree";
            out.best_bound = (model.objective_sense == model::ObjectiveSense::maximize ? -1 : 1) * report.lower_bound;
        }
    } catch (const std::exception& error) {
        out.proof_message = error.what();
        const double elapsed = std::chrono::duration<double, std::milli>(
            Clock::now() - stage_start).count();
        if (building) out.mip_proof_build_ms = elapsed;
        else out.mip_proof_verify_ms = elapsed;
    }
}
} // namespace markov_cero::api::detail
