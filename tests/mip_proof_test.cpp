#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/model/model_snapshot.hpp"
#include "markov_cero/verify/mip_proof.hpp"
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace markov_cero;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

namespace {
std::string fingerprint_of(const model::Model& model) {
    return std::to_string(model::hash_model(model).fingerprint());
}
bool contains(const std::string& text, const char* fragment) {
    return text.find(fragment) != std::string::npos;
}
} // namespace

int main() {
    auto model = io::parse_mps_string(
        "NAME TEST\nROWS\n N COST\n L CAP\nCOLUMNS\n X COST -1 CAP 1\n"
        "RHS\n R CAP 1.5\nBOUNDS\n LI B X 0\n UI B X 2\nENDATA\n");
    api::SolveOptions solve_options;
    solve_options.engine = "milp";
    solve_options.mip_proof_time_limit_seconds = 1e-9;
    const auto timed = api::solve_model(model, solve_options);
    require(timed.status == lp::reference::SolveStatus::feasible, "tiny budget keeps only incumbent status");
    require(timed.proof_status == "exhausted" && timed.proof_budget_exhausted,
            "tiny proof budget exhausted");
    require(timed.proof_budget_kind == "time_limit", "tiny proof budget kind");
    require(timed.mip_proof_build_ms == 0 && timed.proof_budget_time_ms == 0,
            "expired budget skips build and replay");
    solve_options.mip_proof_time_limit_seconds = 5.0;
    const auto roomy = api::solve_model(model, solve_options);
    require(roomy.status == lp::reference::SolveStatus::optimal &&
            roomy.proof_status == "accepted" && roomy.canonical_verified,
            "generous proof budget accepted");
    require(roomy.mip_proof_build_ms > 0 && roomy.proof_budget_time_ms > 0 &&
            roomy.proof_budget_time_ms <= roomy.mip_proof_verify_ms,
            "accepted proof reports separate elapsed times");
    auto proof = verify::build_mip_proof(model, {1}, -1, false);
    require(verify::verify_mip_proof(model, proof).accepted, "baseline proof must be accepted");
    require(proof.nodes.size() == 3, "baseline proof node count");
    require(proof.format_version == verify::kMipProofFormatVersion, "built proof format version");
    require(proof.model_fingerprint.empty(), "built proof starts unbound");
    require(proof.nodes_used == proof.nodes.size(), "build node budget consumption");
    require(proof.witness_values_used > 0, "build witness budget consumption");
    require(proof.budget_time_ms >= 0, "build budget time");
    require(!proof.budget_exhausted, "complete build is not exhausted");
    require(proof.exhausted_budget == verify::MipProofBudgetKind::none, "complete build budget kind");
    const auto baseline = verify::verify_mip_proof(model, proof);
    require(baseline.status == verify::MipProofStatus::accepted, "accepted report status");
    require(baseline.tier == verify::MipAssuranceTier::replayed_tree, "unbound accepted tier");
    require(contains(baseline.message, "model fingerprint unbound"), "unbound message states binding");
    require(!baseline.budget_exhausted, "accepted report budget flag");
    require(baseline.exhausted_budget == verify::MipProofBudgetKind::none, "accepted report budget kind");
    require(baseline.build_ms >= 0 && baseline.replay_ms >= 0, "report build and replay timings");
    require(baseline.build_ms == proof.budget_time_ms &&
            baseline.budget_time_ms == proof.budget_time_ms, "report build budget time");
    require(baseline.nodes_used == proof.nodes_used, "report build node consumption");
    require(baseline.format_version == proof.format_version, "report format version");
    require(baseline.model_fingerprint.empty(), "report unbound fingerprint");
    require(baseline.witness_values_used == proof.witness_values_used, "report build witness consumption");
    require(baseline.checked_witness_values > 0, "replay witness budget consumption");
    require(baseline.checked_nodes == proof.nodes.size(), "replay node consumption");

    const std::string fingerprint = fingerprint_of(model);
    const auto bound = verify::build_mip_proof(model, {1}, -1, false, {}, fingerprint);
    require(!bound.model_fingerprint.empty(), "fingerprint binding stored at build");
    const auto bound_report = verify::verify_mip_proof(model, bound);
    require(bound_report.accepted, "bound proof accepted");
    require(bound_report.tier == verify::MipAssuranceTier::independent_tree, "bound accepted tier");
    require(bound_report.model_fingerprint == fingerprint, "bound report fingerprint");
    auto mismatch = bound;
    mismatch.model_fingerprint = fingerprint + "0";
    const auto mismatch_report = verify::verify_mip_proof(model, mismatch);
    require(!mismatch_report.accepted, "mismatched fingerprint proof rejected");
    require(mismatch_report.status == verify::MipProofStatus::rejected, "mismatch report status");
    require(contains(mismatch_report.message, "fingerprint"), "mismatch message names fingerprint");

    std::stringstream stream;
    verify::write_mip_proof(stream, bound);
    const auto decoded = verify::read_mip_proof(stream);
    require(verify::verify_mip_proof(model, decoded).accepted, "round trip proof accepted");
    require(decoded.format_version == verify::kMipProofFormatVersion, "round trip format version");
    require(decoded.model_fingerprint == bound.model_fingerprint, "round trip fingerprint");
    require(decoded.nodes_used == bound.nodes_used, "round trip nodes_used");
    require(decoded.witness_values_used == bound.witness_values_used, "round trip witness_values_used");
    require(decoded.budget_time_ms == bound.budget_time_ms, "round trip budget_time_ms");
    require(decoded.budget_exhausted == bound.budget_exhausted, "round trip exhausted flag");
    require(decoded.exhausted_budget == bound.exhausted_budget, "round trip exhausted budget kind");
    require(verify::verify_mip_proof(model, decoded).tier == verify::MipAssuranceTier::independent_tree,
            "round trip tier");
    auto annotated = bound;
    verify::record_cut_obligation(annotated, 1, {1.0}, 2.0, 1.5);
    verify::record_propagation_obligation(annotated, 1, 0, 0, 1.0, 0.5, 0.5, true);
    std::stringstream annotated_stream;
    verify::write_mip_proof(annotated_stream, annotated);
    const auto decoded_notes = verify::read_mip_proof(annotated_stream);
    require(decoded_notes.obligations.size() == 2, "obligations round trip");
    const auto built_with_notes = verify::build_mip_proof(model, {1}, -1, false,
        {}, fingerprint, annotated.obligations);
    require(built_with_notes.obligations.size() == 2 &&
            verify::verify_mip_proof(model, built_with_notes).accepted,
            "builder carries optimizer audit notes without changing replay");
    require(decoded_notes.obligations[0].coefficients == std::vector<double>{1.0} &&
            decoded_notes.obligations[1].derived_bound == 0.5,
            "obligation payload round trip");
    const auto note_report = verify::verify_mip_proof(model, decoded_notes);
    require(note_report.accepted && contains(note_report.message, "audit annotations"),
            "annotations do not alter cut-free replay");
    auto stripped = annotated;
    stripped.obligations.clear();
    require(verify::verify_mip_proof(model, stripped).accepted,
            "proof replay survives stripped annotations");
    std::stringstream stripped_stream;
    verify::write_mip_proof(stripped_stream, stripped);
    std::string without_trailer = stripped_stream.str();
    without_trailer.resize(without_trailer.size() - 2); // optional zero-count trailer
    std::stringstream legacy_v3(without_trailer);
    require(verify::read_mip_proof(legacy_v3).obligations.empty(),
            "reader tolerates absent annotation trailer");
    auto unknown = bound;
    unknown.format_version = 7;
    require(verify::verify_mip_proof(model, unknown).status == verify::MipProofStatus::rejected,
            "unknown in-memory proof version rejected");
    try {
        std::stringstream invalid_output;
        verify::write_mip_proof(invalid_output, unknown);
        require(false, "writer accepted unknown proof version");
    } catch (const std::invalid_argument&) {
    }

    auto corrupt = proof;
    corrupt.nodes[0].up = corrupt.nodes[0].down;
    require(!verify::verify_mip_proof(model, corrupt).accepted, "aliasing children rejected");
    corrupt = proof;
    corrupt.nodes[0].split_value = .5;
    require(!verify::verify_mip_proof(model, corrupt).accepted, "fractional split rejected");
    corrupt = proof;
    corrupt.nodes[0].down = 0;
    require(!verify::verify_mip_proof(model, corrupt).accepted, "cyclic node rejected");
    corrupt = proof;
    corrupt.incumbent[0] = 1.5;
    require(!verify::verify_mip_proof(model, corrupt).accepted, "invalid incumbent rejected");
    corrupt = proof;
    corrupt.nodes.emplace_back();
    require(!verify::verify_mip_proof(model, corrupt).accepted, "unreachable record rejected");
    corrupt = proof;
    for (auto& node : corrupt.nodes) node.relaxation.dual.clear();
    require(!verify::verify_mip_proof(model, corrupt).accepted, "empty dual rejected");
    corrupt = proof;
    corrupt.nodes[1].kind = verify::MipProofKind::empty_domain;
    require(!verify::verify_mip_proof(model, corrupt).accepted, "false empty domain rejected");

    verify::MipProofOptions replay_nodes;
    replay_nodes.maximum_nodes = 1;
    const auto replay_node_report = verify::verify_mip_proof(model, proof, replay_nodes);
    require(!replay_node_report.accepted, "replay node budget not accepted");
    require(replay_node_report.status == verify::MipProofStatus::exhausted, "replay node budget status");
    require(replay_node_report.exhausted_budget == verify::MipProofBudgetKind::node_limit,
            "replay node budget kind");
    require(contains(replay_node_report.message, "maximum_nodes"), "replay node budget message");
    verify::MipProofOptions replay_values;
    replay_values.maximum_witness_values = 1;
    const auto replay_value_report = verify::verify_mip_proof(model, proof, replay_values);
    require(!replay_value_report.accepted, "replay witness budget not accepted");
    require(replay_value_report.status == verify::MipProofStatus::exhausted, "replay witness budget status");
    require(replay_value_report.exhausted_budget == verify::MipProofBudgetKind::witness_limit,
            "replay witness budget kind");
    require(contains(replay_value_report.message, "maximum_witness_values"), "replay witness budget message");
    verify::MipProofOptions replay_deadline;
    replay_deadline.deadline = std::chrono::steady_clock::now();
    const auto replay_deadline_report = verify::verify_mip_proof(model, proof, replay_deadline);
    require(!replay_deadline_report.accepted, "replay deadline not accepted");
    require(replay_deadline_report.status == verify::MipProofStatus::exhausted, "replay deadline status");
    require(replay_deadline_report.exhausted_budget == verify::MipProofBudgetKind::time_limit,
            "replay deadline budget kind");
    require(contains(replay_deadline_report.message, "deadline"), "replay deadline message");
    require(replay_deadline_report.tier == verify::MipAssuranceTier::unverified, "exhausted tier");

    verify::MipProofOptions starve_nodes;
    starve_nodes.maximum_nodes = 1;
    const auto starved = verify::build_mip_proof(model, {1}, -1, false, starve_nodes);
    require(starved.budget_exhausted, "build node budget exhausted");
    require(starved.exhausted_budget == verify::MipProofBudgetKind::node_limit, "build node budget kind");
    require(starved.nodes_used == starved.nodes.size() && starved.nodes_used == 1, "starved build nodes_used");
    const auto starved_report = verify::verify_mip_proof(model, starved, starve_nodes);
    require(!starved_report.accepted, "starved proof not accepted");
    require(starved_report.status == verify::MipProofStatus::exhausted, "starved report status");
    require(starved_report.budget_exhausted, "starved report budget flag");
    require(starved_report.exhausted_budget == verify::MipProofBudgetKind::node_limit, "starved report kind");
    require(starved_report.tier == verify::MipAssuranceTier::unverified, "starved report tier");
    require(starved_report.nodes_used == starved.nodes_used, "starved report nodes used");
    require(starved_report.witness_values_used == starved.witness_values_used,
            "starved report witness values used");
    require(starved_report.budget_time_ms == starved.budget_time_ms, "starved report budget time");
    require(contains(starved_report.message, "maximum_nodes"), "starved report message");
    std::stringstream starved_stream;
    verify::write_mip_proof(starved_stream, starved);
    const auto starved_decoded = verify::read_mip_proof(starved_stream);
    require(starved_decoded.budget_exhausted &&
            starved_decoded.exhausted_budget == verify::MipProofBudgetKind::node_limit,
            "exhausted proof round trip");

    verify::MipProofOptions starve_values;
    starve_values.maximum_witness_values = 1;
    const auto values_starved = verify::build_mip_proof(model, {1}, -1, false, starve_values);
    require(values_starved.budget_exhausted, "build witness budget exhausted");
    require(values_starved.exhausted_budget == verify::MipProofBudgetKind::witness_limit,
            "build witness budget kind");
    const auto values_report = verify::verify_mip_proof(model, values_starved);
    require(values_report.status == verify::MipProofStatus::exhausted, "witness starved status");
    require(values_report.exhausted_budget == verify::MipProofBudgetKind::witness_limit, "witness starved kind");
    require(contains(values_report.message, "maximum_witness_values"), "witness starved message");

    verify::MipProofOptions starve_time;
    starve_time.deadline = std::chrono::steady_clock::now();
    const auto time_starved = verify::build_mip_proof(model, {1}, -1, false, starve_time);
    require(time_starved.budget_exhausted, "build deadline exhausted");
    require(time_starved.exhausted_budget == verify::MipProofBudgetKind::time_limit, "build deadline kind");
    const auto time_report = verify::verify_mip_proof(model, time_starved);
    require(time_report.status == verify::MipProofStatus::exhausted, "deadline starved status");
    require(time_report.exhausted_budget == verify::MipProofBudgetKind::time_limit, "deadline starved kind");
    require(contains(time_report.message, "deadline"), "deadline starved message");

    auto nonlinear = model;
    nonlinear.has_nlobj_section = true;
    const auto unsupported = verify::verify_mip_proof(nonlinear, proof);
    require(!unsupported.accepted, "unsupported model not accepted");
    require(unsupported.status == verify::MipProofStatus::unsupported, "unsupported model status");
    require(unsupported.tier == verify::MipAssuranceTier::unverified, "unsupported model tier");
    require(contains(unsupported.message, "linear and convex quadratic"), "unsupported model message");

    const auto no_proof = verify::build_mip_proof(model, {0}, 0, false);
    require(!verify::verify_mip_proof(model, no_proof).accepted, "gap not closed");
    model.row_lower[0] = model::Bound::finite(.5);
    model.row_upper[0] = model::Bound::finite(.5);
    const auto infeasible = verify::build_mip_proof(model, {}, 0, true);
    require(verify::verify_mip_proof(model, infeasible).accepted, "infeasible proof accepted");

    const auto quadratic = io::parse_mps_string(
        "NAME QINT\nROWS\n N COST\nCOLUMNS\n X COST -1\nBOUNDS\n LI B X 0\n UI B X 2\n"
        "QUADOBJ\n X X 2\nENDATA\n");
    auto qp_proof = verify::build_mip_proof(quadratic, {0}, 0, false);
    auto qp_report = verify::verify_mip_proof(quadratic, qp_proof);
    if (!qp_report.accepted) throw std::runtime_error(qp_report.message);
    require(qp_proof.nodes.size() == 3, "quadratic proof node count");
    std::stringstream qp_stream;
    verify::write_mip_proof(qp_stream, qp_proof);
    require(verify::verify_mip_proof(quadratic, verify::read_mip_proof(qp_stream)).accepted,
            "quadratic round trip accepted");
    for (auto& node : qp_proof.nodes) node.relaxation.dual.assign(node.relaxation.dual.size(), 100);
    require(!verify::verify_mip_proof(quadratic, qp_proof).accepted, "corrupted dual rejected");

    std::stringstream expired_stream;
    verify::write_mip_proof(expired_stream, proof);
    verify::MipProofOptions expired;
    expired.deadline = std::chrono::steady_clock::now();
    bool parsing_expired = false;
    try {
        (void)verify::read_mip_proof(expired_stream, expired);
    } catch (const std::runtime_error&) {
        parsing_expired = true;
    }
    require(parsing_expired, "proof parsing deadline");
    for (const auto text : {"MARKOV_MIP_PROOF 1 0 1 3", "MARKOV_MIP_PROOF 7 0 1 3",
                            "MARKOV_MIP_PROOF 3 0 0 999999999", "MARKOV_MIP_PROOF 3 0 nan 1",
                            "MARKOV_MIP_PROOF 3 0 1 3"}) {
        bool rejected = false;
        try {
            std::stringstream bad(text);
            (void)verify::read_mip_proof(bad);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "invalid or unsupported proof header");
    }
    try {
        std::stringstream version("MARKOV_MIP_PROOF 7 0 1 3");
        (void)verify::read_mip_proof(version);
        require(false, "unknown proof format version accepted");
    } catch (const std::invalid_argument& error) {
        require(contains(error.what(), "unsupported MIP proof format version"), "version error text");
    }
    try {
        std::stringstream budget("MARKOV_MIP_PROOF 3 0 1 3 0 0 0 1 0");
        (void)verify::read_mip_proof(budget);
        require(false, "inconsistent budget record accepted");
    } catch (const std::invalid_argument&) {
    }
}
