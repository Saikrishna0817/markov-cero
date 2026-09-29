#include "api_internal.hpp"
#include "markov_cero/verify/mip_proof.hpp"
namespace markov_cero::api::detail {
namespace {
const char* tier_name(verify::MipAssuranceTier tier) {
    switch (tier) {
    case verify::MipAssuranceTier::independent_tree: return "independent_tree";
    case verify::MipAssuranceTier::replayed_tree: return "replayed_tree";
    case verify::MipAssuranceTier::unverified: return "unverified";
    }
    return "unverified";
}
const char* status_name(verify::MipProofStatus status) {
    switch (status) {
    case verify::MipProofStatus::accepted: return "accepted";
    case verify::MipProofStatus::exhausted: return "exhausted";
    case verify::MipProofStatus::rejected: return "rejected";
    case verify::MipProofStatus::unsupported: return "unsupported";
    }
    return "rejected";
}
const char* budget_name(verify::MipProofBudgetKind kind) {
    switch (kind) {
    case verify::MipProofBudgetKind::none: return "none";
    case verify::MipProofBudgetKind::time_limit: return "time_limit";
    case verify::MipProofBudgetKind::node_limit: return "node_limit";
    case verify::MipProofBudgetKind::witness_limit: return "witness_limit";
    }
    return "none";
}
void copy_proof_report(const verify::MipProofReport& report, SolveResult& out) {
    out.guarantee_tier = tier_name(report.tier);
    out.proof_status = status_name(report.status);
    out.proof_budget_exhausted = report.budget_exhausted;
    out.proof_budget_kind = budget_name(report.exhausted_budget);
    out.proof_nodes_used = report.nodes_used;
    out.proof_checked_nodes = report.checked_nodes;
    out.proof_witness_values_used = report.witness_values_used;
    out.proof_checked_witness_values = report.checked_witness_values;
    out.proof_budget_time_ms = report.replay_ms;
    out.proof_format_version = report.format_version;
    out.proof_model_fingerprint = report.model_fingerprint;
}
} // namespace
void certify_mip(const model::Model& model, const SolveOptions& options, SolveResult& out,
                 lp::reference::Result& result, core::SolveContext& ctx,
                 const std::vector<verify::MipObligation>& obligations) {
    using Status = lp::reference::SolveStatus;
    if (!options.enable_mip_proof ||
        (result.status != Status::optimal && result.status != Status::gap_satisfied &&
         result.status != Status::infeasible)) return;
    const auto entered = Clock::now();
    verify::MipProofOptions proof_options;
    proof_options.deadline = entered + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(options.mip_proof_time_limit_seconds));
    if (options.lp_options.deadline && *options.lp_options.deadline < *proof_options.deadline)
        proof_options.deadline = options.lp_options.deadline;
    proof_options.maximum_nodes = options.mip_proof_max_nodes;
    proof_options.maximum_witness_values = options.mip_proof_max_witness_values;
    proof_options.relative_gap = result.status == Status::gap_satisfied
        ? options.milp_options.relative_gap_tolerance : 0;
    if (Clock::now() >= *proof_options.deadline) {
        out.proof_status = "exhausted";
        out.proof_budget_exhausted = true;
        out.proof_budget_kind = "time_limit";
        out.proof_message = "proof build exhausted time budget (deadline) before starting";
        return;
    }
    const std::size_t proof_bytes = model.matrix.value.size() *
        (sizeof(double) + sizeof(std::size_t)) +
        std::min<std::size_t>(options.mip_proof_max_nodes, 1024U) * 64U + 4096U;
    if (!charge_or_fail(ctx, proof_bytes, "proof_build", out, result)) return;
    auto stage_start = Clock::now();
    bool building = true;
    try {
        std::shared_ptr<verify::MipProof> proof;
        {
            core::StageScope stage(ctx, "proof_build");
            proof = std::make_shared<verify::MipProof>(verify::build_mip_proof(model,
                out.original_primal, out.original_objective, result.status == Status::infeasible,
                proof_options, std::to_string(out.model_fingerprint), obligations));
        }
        const auto verify_start = Clock::now();
        out.mip_proof_build_ms = std::chrono::duration<double, std::milli>(
            verify_start - stage_start).count();
        stage_start = verify_start;
        building = false;
        out.proof_status = proof->budget_exhausted ? "exhausted" : "not_replayed";
        out.proof_budget_exhausted = proof->budget_exhausted;
        out.proof_budget_kind = budget_name(proof->exhausted_budget);
        out.proof_nodes_used = proof->nodes_used;
        out.proof_witness_values_used = proof->witness_values_used;
        out.proof_budget_time_ms = 0.0;
        out.proof_format_version = proof->format_version;
        out.proof_model_fingerprint = proof->model_fingerprint;
        out.mip_proof = proof;
        core::StageScope replay(ctx, "proof_replay");
        const auto report = verify::verify_mip_proof(model, *proof, proof_options);
        out.mip_proof_verify_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - verify_start).count();
        copy_proof_report(report, out);
        out.proof_message = report.message;
        if (stop_after_deadline(ctx, options, out, result, "MIP proof replay")) return;
        out.canonical_verified = report.accepted && !report.budget_exhausted &&
            report.tier == verify::MipAssuranceTier::independent_tree;
        if (out.canonical_verified) {
            // A gap proof certifies the requested bound, not exact optimality.
            out.certificate_type = result.status == Status::gap_satisfied
                ? "independent_mip_gap" : "independent_mip_tree";
            out.best_bound = (model.objective_sense == model::ObjectiveSense::maximize ? -1 : 1) * report.lower_bound;
        } else if (report.accepted) {
            out.certificate_type = "replayed_mip_tree";
        }
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::length_error&) {
        throw;
    } catch (const std::exception& error) {
        out.proof_message = error.what();
        out.proof_status = "rejected";
        out.guarantee_tier = "unverified";
        out.canonical_verified = false;
        const double elapsed = std::chrono::duration<double, std::milli>(
            Clock::now() - stage_start).count();
        if (building) out.mip_proof_build_ms = elapsed;
        else out.mip_proof_verify_ms = elapsed;
    }
}
} // namespace markov_cero::api::detail
