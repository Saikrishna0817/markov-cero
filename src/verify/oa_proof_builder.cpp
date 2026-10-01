// MINLP-02 (docs/contracts/minlp-proof-replay.md §4.3): generator side of
// the OA proof. Re-derives every cut from the source, rebuilds the master
// with the verifier-side assembly and embeds a cut-free build_mip_proof tree
// of that master — byte-identical inputs on both sides of the check.
#include "oa_proof_internal.hpp"
#include "markov_cero/verify/oa_proof.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace markov_cero::verify {
OaProof build_oa_proof(const model::Model& source, const minlp::MinlpSolution& solution,
                       const OaProofOptions& options, const std::string& model_fingerprint) {
    if (solution.master_history.empty())
        throw std::invalid_argument("oa proof: no master revision was captured");
    if (solution.oa_cuts.size() > options.maximum_tangents)
        throw OaBudgetExhausted("oa proof: tangent count " +
                                std::to_string(solution.oa_cuts.size()) +
                                " exceeds maximum_tangents " +
                                std::to_string(options.maximum_tangents));
    if (options.deadline && std::chrono::steady_clock::now() >= *options.deadline)
        throw OaBudgetExhausted("oa proof build exhausted time budget (deadline)");

    OaProof proof;
    proof.format_version = kOaProofFormatVersion;
    proof.model_fingerprint = model_fingerprint;
    proof.source_sense = source.objective_sense == model::ObjectiveSense::maximize
                             ? OaProofSense::maximize
                             : OaProofSense::minimize;
    const bool infeasible = solution.status == lp::reference::SolveStatus::infeasible;
    proof.kind = infeasible ? OaProofKind::infeasible : OaProofKind::optimal;
    // Contract §2.1: claimed values are source-sense; infeasible proofs
    // carry exact zeros and an empty incumbent.
    const double sign = oa_detail::sense_sign(source);
    proof.claimed_objective = infeasible ? 0.0 : sign * solution.objective;
    proof.claimed_best_bound = infeasible ? 0.0 : sign * solution.best_bound;
    proof.claimed_relative_gap = infeasible ? 0.0 : solution.relative_gap;
    proof.incumbent = infeasible ? std::vector<double>{} : solution.x;
    proof.master_history = solution.master_history;
    proof.convexity_pivots = oa_detail::convexity_pivots(source); // may throw invalid_argument
    proof.cuts = solution.oa_cuts;

    // O5/O6: rebuild the master from re-derived cuts (not the captured
    // solve-side model) and embed its independent tree proof.
    model::Model master = oa_detail::rebuild_master(source, proof.cuts);
    const std::string master_fingerprint = mip_detail::bound_fingerprint(master);
    MipProofOptions mip_options;
    mip_options.maximum_nodes = options.maximum_nodes;
    mip_options.maximum_witness_values = options.maximum_witness_values;
    mip_options.relative_gap = options.relative_gap;
    mip_options.deadline = options.deadline;
    proof.master_proof = build_mip_proof(
        master, infeasible ? std::vector<double>{} : solution.master_primal,
        infeasible ? 0.0 : solution.master_objective, infeasible, mip_options,
        master_fingerprint, {});
    return proof;
}
} // namespace markov_cero::verify
