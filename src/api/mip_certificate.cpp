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
    proof_options.deadline = Clock::now() + std::chrono::seconds(5);
    if (options.lp_options.deadline && *options.lp_options.deadline < *proof_options.deadline)
        proof_options.deadline = options.lp_options.deadline;
    proof_options.relative_gap = result.status == Status::gap_satisfied
        ? options.milp_options.relative_gap_tolerance : 0;
    try {
        auto proof = std::make_shared<verify::MipProof>(verify::build_mip_proof(model,
            out.original_primal, out.original_objective, result.status == Status::infeasible, proof_options));
        const auto report = verify::verify_mip_proof(model, *proof, proof_options);
        out.mip_proof = std::move(proof);
        out.proof_message = report.message;
        out.canonical_verified = report.accepted;
        if (report.accepted) {
            out.certificate_type = "independent_mip_tree";
            out.best_bound = (model.objective_sense == model::ObjectiveSense::maximize ? -1 : 1) * report.lower_bound;
        }
    } catch (const std::exception& error) { out.proof_message = error.what(); }
}
} // namespace markov_cero::api::detail
