#include "api_internal.hpp"
#include "markov_cero/verify/oa_proof.hpp"
namespace markov_cero::api::detail {
namespace {
const char* oa_tier_name(verify::OaAssuranceTier tier) {
    switch (tier) {
    case verify::OaAssuranceTier::independent_oa: return "independent_oa";
    case verify::OaAssuranceTier::replayed_oa: return "replayed_oa";
    case verify::OaAssuranceTier::unverified: return "unverified";
    }
    return "unverified";
}
const char* oa_status_name(verify::MipProofStatus status) {
    switch (status) {
    case verify::MipProofStatus::accepted: return "accepted";
    case verify::MipProofStatus::exhausted: return "exhausted";
    case verify::MipProofStatus::rejected: return "rejected";
    case verify::MipProofStatus::unsupported: return "unsupported";
    }
    return "rejected";
}
const char* mip_budget_name(verify::MipProofBudgetKind kind) {
    switch (kind) {
    case verify::MipProofBudgetKind::none: return "none";
    case verify::MipProofBudgetKind::time_limit: return "time_limit";
    case verify::MipProofBudgetKind::node_limit: return "node_limit";
    case verify::MipProofBudgetKind::witness_limit: return "witness_limit";
    }
    return "none";
}
const char* oa_budget_name(verify::OaBudgetKind kind) {
    switch (kind) {
    case verify::OaBudgetKind::none: return "none";
    case verify::OaBudgetKind::time_limit: return "time_limit";
    case verify::OaBudgetKind::node_limit: return "node_limit";
    case verify::OaBudgetKind::witness_limit: return "witness_limit";
    case verify::OaBudgetKind::tangent_limit: return "tangent_limit";
    }
    return "none";
}
void copy_oa_report(const verify::OaProofReport& report, SolveResult& out) {
    out.guarantee_tier = oa_tier_name(report.tier);
    out.proof_status = oa_status_name(report.status);
    out.proof_budget_exhausted = report.budget_exhausted;
    out.proof_budget_kind = oa_budget_name(report.exhausted_budget);
    out.proof_nodes_used = report.nodes_used;
    out.proof_checked_nodes = report.checked_nodes;
    out.proof_witness_values_used = report.witness_values_used;
    out.proof_checked_witness_values = report.checked_witness_values;
    out.proof_budget_time_ms = report.replay_ms;
    out.proof_format_version = report.format_version;
    out.proof_model_fingerprint = report.model_fingerprint;
}
} // namespace
void certify_minlp(const model::Model& model, const SolveOptions& options, SolveResult& out,
                   lp::reference::Result& result, core::SolveContext& ctx,
                   const minlp::MinlpSolution& sol) {
    using Status = lp::reference::SolveStatus;
    if (!options.enable_mip_proof ||
        (result.status != Status::optimal && result.status != Status::gap_satisfied &&
         result.status != Status::infeasible)) return;
    const auto entered = Clock::now();
    verify::OaProofOptions proof_options;
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
        out.proof_message = "OA proof build exhausted time budget (deadline) before starting";
        return;
    }
    const std::size_t proof_bytes = model.matrix.value.size() *
        (sizeof(double) + sizeof(std::size_t)) +
        std::min<std::size_t>(options.mip_proof_max_nodes, 1024U) * 64U + 4096U;
    if (!charge_or_fail(ctx, proof_bytes, "proof_build", out, result)) return;
    // RES-01: honor a solve-wide stop recorded before the build starts; the
    // proof budget itself stays a proof outcome (contract §4.2).
    if (stop_after_deadline(ctx, options, out, result, "OA proof build")) return;
    auto stage_start = Clock::now();
    bool building = true;
    try {
        std::shared_ptr<verify::OaProof> proof;
        {
            core::StageScope stage(ctx, "proof_build");
            proof = std::make_shared<verify::OaProof>(verify::build_oa_proof(
                model, sol, proof_options, std::to_string(out.model_fingerprint)));
        }
        const auto verify_start = Clock::now();
        out.oa_proof_build_ms = std::chrono::duration<double, std::milli>(
            verify_start - stage_start).count();
        stage_start = verify_start;
        building = false;
        out.proof_status = proof->master_proof.budget_exhausted ? "exhausted" : "not_replayed";
        out.proof_budget_exhausted = proof->master_proof.budget_exhausted;
        out.proof_budget_kind = proof->master_proof.budget_exhausted
            ? mip_budget_name(proof->master_proof.exhausted_budget) : "none";
        out.proof_nodes_used = proof->master_proof.nodes_used;
        out.proof_witness_values_used = proof->master_proof.witness_values_used;
        out.proof_budget_time_ms = 0.0;
        out.proof_format_version = proof->format_version;
        out.proof_model_fingerprint = proof->model_fingerprint;
        out.oa_proof = proof;
        core::StageScope replay(ctx, "proof_replay");
        const auto report = verify::verify_oa_proof(model, *proof, proof_options);
        out.oa_proof_verify_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - verify_start).count();
        copy_oa_report(report, out);
        out.proof_message = report.message;
        if (stop_after_deadline(ctx, options, out, result, "OA proof replay")) return;
        // Contract §5.2: only an accepted independent replay is canonical.
        out.canonical_verified = report.accepted && !report.budget_exhausted &&
            report.tier == verify::OaAssuranceTier::independent_oa;
        if (out.canonical_verified) {
            out.certificate_type = sol.relative_gap == 0.0
                ? "incumbent_feasibility; independent_oa_tree"
                : "incumbent_feasibility; independent_oa_gap";
            // The published bound/gap are the replayed ones (contract §5.2).
            const double sign = model.objective_sense == model::ObjectiveSense::maximize
                ? -1.0 : 1.0;
            out.best_bound = sign * report.lower_bound;
            out.relative_gap = report.relative_gap;
            out.original_message =
                "MINLP incumbent feasibility verified; OA bound independently replayed";
        } else if (report.accepted) {
            out.certificate_type = "replayed_oa_bound";
        }
    } catch (const verify::OaBudgetExhausted& error) {
        out.proof_message = error.what();
        out.proof_status = "exhausted";
        out.proof_budget_exhausted = true;
        out.proof_budget_kind = std::string(error.what()).find("tangent") != std::string::npos
            ? "tangent_limit" : "time_limit";
        out.guarantee_tier = "unverified";
        out.canonical_verified = false;
        if (building) out.oa_proof_build_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - stage_start).count();
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
        if (building) out.oa_proof_build_ms = elapsed;
        else out.oa_proof_verify_ms = elapsed;
    }
}
} // namespace markov_cero::api::detail
