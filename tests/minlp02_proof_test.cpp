// MINLP-02 contract §7 (docs/contracts/minlp-proof-replay.md): positive proof
// fixtures (case A, infeasible, maximize), §5.3 downgrade/budget behavior and
// the source-blind reader rejections. Every proof below is round-tripped
// through write -> read -> verify_oa_proof before it is trusted.

#include "minlp02_common.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace markov_cero;
using minlp02::require;

namespace {
verify::OaProofReport roundtrip(const model::Model& source, const verify::OaProof& proof) {
    std::istringstream in(minlp02::serialize(proof));
    return verify::verify_oa_proof(source, verify::read_oa_proof(in));
}

void expect_read_rejects(const std::string& text, const char* what,
                         const verify::OaProofOptions& limits = {}) {
    std::istringstream in(text);
    try {
        (void)verify::read_oa_proof(in, limits);
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(std::string("reader accepted ") + what);
}

void require_positive(const model::Model& source, const api::SolveResult& res,
                      const char* label) {
    minlp02::require(res.status == lp::reference::SolveStatus::optimal, label);
    minlp02::require(res.assurance == "oa_replayed", label);
    minlp02::require(res.guarantee_tier == "independent_oa", label);
    minlp02::require(res.proof_status == "accepted", label);
    minlp02::require(res.verified && res.canonical_verified && res.original_verified, label);
    minlp02::require(res.certificate_type.find("independent_oa") != std::string::npos, label);
    minlp02::require(res.oa_proof != nullptr, label);
    minlp02::require(res.oa_proof->format_version == verify::kOaProofFormatVersion, label);
    minlp02::require(res.proof_format_version == verify::kOaProofFormatVersion, label);
    minlp02::require(res.oa_proof->model_fingerprint == minlp02::fingerprint_of(source), label);
    minlp02::require(res.proof_model_fingerprint == res.oa_proof->model_fingerprint, label);
    minlp02::require(res.oa_proof_build_ms >= 0.0 && res.oa_proof_verify_ms >= 0.0, label);
    const auto report = roundtrip(source, *res.oa_proof);
    minlp02::require(report.accepted &&
                         report.tier == verify::OaAssuranceTier::independent_oa,
                     label);
    const double sign =
        source.objective_sense == model::ObjectiveSense::maximize ? -1.0 : 1.0;
    minlp02::require(
        std::abs(res.best_bound - sign * report.lower_bound) <=
            1e-9 * (1.0 + std::abs(res.best_bound)),
        label);
    minlp02::require(std::abs(res.relative_gap - report.relative_gap) <= 1e-12, label);
}
} // namespace

int main() {
    // Positive fixture: healthy case A (minlp-oa.md §11).
    const auto source = minlp02::case_a_source();
    const auto case_a = api::solve_model(source, {});
    require_positive(source, case_a, "case A positive fixture");
    minlp02::require(std::abs(case_a.objective - 0.2) <= 1e-6, "case A objective");

    // Positive fixture: structurally infeasible member stays Infeasible with
    // an accepted infeasible proof and canonical verification.
    const auto infeasible_source = minlp02::infeasible_source();
    const auto infeasible = api::solve_model(infeasible_source, {});
    minlp02::require(infeasible.status == lp::reference::SolveStatus::infeasible,
                     "infeasible fixture status");
    minlp02::require(infeasible.verified && infeasible.canonical_verified,
                     "infeasible fixture canonical");
    minlp02::require(infeasible.assurance == "oa_replayed", "infeasible assurance");
    minlp02::require(infeasible.oa_proof != nullptr, "infeasible proof attached");
    minlp02::require(infeasible.oa_proof->kind == verify::OaProofKind::infeasible,
                     "infeasible proof kind");
    minlp02::require(infeasible.oa_proof->master_proof.claims_infeasible,
                     "infeasible proof embeds claim");
    minlp02::require(infeasible.oa_proof->incumbent.empty() &&
                          infeasible.oa_proof->claimed_objective == 0.0 &&
                          infeasible.oa_proof->claimed_best_bound == 0.0,
                     "infeasible proof carries no claim");
    minlp02::require(infeasible.certificate_type.find("independent_oa") !=
                          std::string::npos,
                     "infeasible certificate");
    minlp02::require(roundtrip(infeasible_source, *infeasible.oa_proof).accepted,
                     "infeasible roundtrip");

    // Positive fixture: maximize source maps objective/bound/gap through the
    // sense sign on both build and replay.
    const auto maximize_source = minlp02::maximize_source();
    const auto maximize = api::solve_model(maximize_source, {});
    require_positive(maximize_source, maximize, "maximize positive fixture");
    minlp02::require(std::abs(maximize.objective + 1.25) <= 1e-6,
                     "maximize objective sign");
    minlp02::require(std::abs(maximize.best_bound + 1.25) <= 1e-6,
                     "maximize bound sign");
    minlp02::require(maximize.oa_proof->source_sense == verify::OaProofSense::maximize,
                     "maximize declared sense");

    // Budget: a starved node budget exhausts the replay (attached build),
    // never reports Optimal and never becomes canonical.
    api::SolveOptions starved;
    starved.mip_proof_max_nodes = 1;
    const auto nodes_limited = api::solve_model(source, starved);
    minlp02::require(nodes_limited.status == lp::reference::SolveStatus::feasible,
                     "node budget downgrades off Optimal");
    minlp02::require(nodes_limited.proof_status == "exhausted" &&
                          nodes_limited.proof_budget_exhausted &&
                          nodes_limited.proof_budget_kind == "node_limit",
                     "node budget exhaustion reported");
    minlp02::require(!nodes_limited.verified && !nodes_limited.canonical_verified,
                     "exhaustion is not verified");
    minlp02::require(nodes_limited.guarantee_tier == "unverified",
                     "exhaustion tier unverified");
    minlp02::require(minlp02::contains(nodes_limited.message, "independent proof exhausted"),
                     "exhaustion message names proof status");
    minlp02::require(nodes_limited.oa_proof != nullptr, "exhausted replay keeps build");

    // Budget: an expired time limit skips the build entirely.
    api::SolveOptions expired;
    expired.mip_proof_time_limit_seconds = 1e-9;
    const auto time_limited = api::solve_model(source, expired);
    minlp02::require(time_limited.status == lp::reference::SolveStatus::feasible,
                     "time budget downgrades off Optimal");
    minlp02::require(time_limited.proof_status == "exhausted" &&
                          time_limited.proof_budget_kind == "time_limit",
                     "time budget exhaustion reported");
    minlp02::require(time_limited.oa_proof == nullptr && time_limited.oa_proof_build_ms == 0.0,
                     "expired budget skips build");
    minlp02::require(
        minlp02::contains(time_limited.message, "independent proof exhausted"),
        "time exhaustion message names proof status");

    // Disabled proofs mirror the not_requested downgrade (§5.3).
    api::SolveOptions disabled;
    disabled.enable_mip_proof = false;
    const auto off = api::solve_model(source, disabled);
    minlp02::require(off.status == lp::reference::SolveStatus::feasible,
                     "disabled proofs downgrade off Optimal");
    minlp02::require(off.proof_status == "not_requested" && off.oa_proof == nullptr &&
                          !off.canonical_verified,
                     "disabled proof fields");
    minlp02::require(minlp02::contains(off.message, "independent proof not_requested"),
                     "disabled message names proof status");

    // A stripped fingerprint verifies only at the replayed_oa tier.
    auto stripped = *case_a.oa_proof;
    stripped.model_fingerprint.clear();
    const auto unbound = roundtrip(source, stripped);
    minlp02::require(unbound.accepted, "stripped fingerprint still replays");
    minlp02::require(unbound.tier == verify::OaAssuranceTier::replayed_oa,
                     "stripped fingerprint tier");

    // Reader rejections (contract §7): every malformed artifact throws.
    const std::string text = minlp02::serialize(*case_a.oa_proof);
    minlp02::require(text.rfind("MARKOV_OA_PROOF 1\n", 0) == 0, "serialized header");
    expect_read_rejects(text + "\nEXTRA\n", "trailing data");
    expect_read_rejects(text.substr(0, text.size() - 10), "truncated stream");
    std::string version2 = text;
    version2.replace(0, 17, "MARKOV_OA_PROOF 2");
    expect_read_rejects(version2, "version 2");
    verify::OaProofOptions tight;
    tight.maximum_tangents = 1;
    expect_read_rejects(text, "tangent count over cap", tight);
    std::string bad_mip = text;
    const auto mip_at = bad_mip.find("MARKOV_MIP_PROOF");
    minlp02::require(mip_at != std::string::npos, "embedded MIP block present");
    bad_mip.replace(mip_at, 18, "MARKOV_MIP_PROOF 9");
    expect_read_rejects(bad_mip, "malformed embedded MIP block");
    auto non_finite = *case_a.oa_proof;
    non_finite.claimed_objective = std::numeric_limits<double>::quiet_NaN();
    expect_read_rejects(minlp02::serialize(non_finite), "non-finite claim scalar");

    std::cout << "MINLP-02 proof fixtures, downgrades and reader rejections passed\n";
}
