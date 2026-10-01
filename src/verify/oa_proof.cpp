// MINLP-02 (docs/contracts/minlp-proof-replay.md §3): deterministic OA proof
// replay, obligations O1-O12 in order. No solver is ever called here; every
// bound comes from the embedded witness tree, every row from the source
// polynomial, every feasibility claim from the shared verifiers.
#include "oa_proof_internal.hpp"
#include "markov_cero/io/nlobj_parser.hpp"
#include "markov_cero/nlp/nlp_verifier.hpp"
#include "markov_cero/verify/oa_proof.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace markov_cero::verify {
namespace {
using Clock = std::chrono::steady_clock;

const char* history_status_is_certified(lp::reference::SolveStatus status) {
    // minlp-oa.md §7.1 certified master set (contract O7).
    switch (status) {
    case lp::reference::SolveStatus::optimal:
    case lp::reference::SolveStatus::gap_satisfied:
    case lp::reference::SolveStatus::feasible:
    case lp::reference::SolveStatus::iteration_limit:
    case lp::reference::SolveStatus::resource_limit: return "certified";
    default: return "";
    }
}
} // namespace

OaProofReport verify_oa_proof(const model::Model& source, const OaProof& proof,
                              const OaProofOptions& options) {
    OaProofReport report;
    report.format_version = proof.format_version;
    report.model_fingerprint = proof.model_fingerprint;
    const auto started = Clock::now();
    const auto expired = [&] {
        return options.deadline && Clock::now() >= *options.deadline;
    };
    const auto finish = [&](OaProofReport out) {
        out.replay_ms =
            std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        return out;
    };
    const auto fail = [&](const std::string& message) {
        OaProofReport out = report;
        out.accepted = false;
        out.status = MipProofStatus::rejected;
        out.tier = OaAssuranceTier::unverified;
        out.message = message;
        return finish(out);
    };
    const auto exhaust = [&](OaBudgetKind kind, const std::string& message) {
        OaProofReport out = report;
        out.accepted = false;
        out.status = MipProofStatus::exhausted;
        out.tier = OaAssuranceTier::unverified;
        out.budget_exhausted = true;
        out.exhausted_budget = kind;
        out.message = message;
        return finish(out);
    };
    try {
        // O1 — structure (reader rules, re-checked for in-memory records).
        if (proof.format_version != kOaProofFormatVersion)
            return fail("o1 format version mismatch");
        if (proof.kind != OaProofKind::optimal && proof.kind != OaProofKind::infeasible)
            return fail("o1 kind out of range");
        if (proof.source_sense != OaProofSense::minimize && proof.source_sense != OaProofSense::maximize)
            return fail("o1 sense out of range");
        if (proof.model_fingerprint.size() > kOaProofFingerprintLimit)
            return fail("o1 fingerprint over limit");
        for (double value : {proof.claimed_objective, proof.claimed_best_bound,
                             proof.claimed_relative_gap})
            if (!std::isfinite(value)) return fail("o1 non-finite claim");
        // O2 — model binding: fingerprint and declared source sense (empty
        // fingerprint => accepted at replayed_oa tier only).
        bool fingerprint_bound = true;
        const OaProofSense source_sense =
            oa_detail::sense_sign(source) < 0 ? OaProofSense::maximize
                                              : OaProofSense::minimize;
        if (proof.source_sense != source_sense)
            return fail("o2 source sense does not match the verified model");
        if (proof.model_fingerprint.empty()) {
            fingerprint_bound = false;
        } else if (proof.model_fingerprint != mip_detail::bound_fingerprint(source)) {
            return fail("o2 model fingerprint mismatch");
        }
        if (expired()) return exhaust(OaBudgetKind::time_limit, "o11 replay deadline reached");
        // O3 — convexity evidence recomputed from the source.
        std::vector<double> pivots;
        try {
            pivots = oa_detail::convexity_pivots(source);
        } catch (const std::exception& error) {
            return fail(std::string("o3 convexity: ") + error.what());
        }
        if (pivots.size() != proof.convexity_pivots.size())
            return fail("o3 convexity evidence count mismatch");
        for (std::size_t k = 0; k < pivots.size(); ++k) {
            const double tolerance = 1e-6 * std::max(1.0, std::abs(proof.convexity_pivots[k]));
            if (std::abs(pivots[k] - proof.convexity_pivots[k]) > tolerance)
                return fail("o3 convexity evidence mismatch");
        }
        // O4 — tangents re-derived from the source polynomial.
        try {
            const auto cut_report = minlp::replay_oa_cuts(source, proof.cuts);
            report.checked_cuts = cut_report.checked;
            if (!cut_report.accepted) return fail("o4 tangent replay: " + cut_report.message);
        } catch (const std::exception& error) {
            return fail(std::string("o4 tangent replay: ") + error.what());
        }
        if (expired()) return exhaust(OaBudgetKind::time_limit, "o11 replay deadline reached");
        // O5 — master assembly bound to the embedded tree fingerprint.
        model::Model master;
        try {
            master = oa_detail::rebuild_master(source, proof.cuts);
        } catch (const std::exception& error) {
            return fail(std::string("o5 master assembly: ") + error.what());
        }
        if (mip_detail::bound_fingerprint(master) != proof.master_proof.model_fingerprint)
            return fail("o5 master assembly fingerprint mismatch");
        // O6 — embedded cut-free master tree; no solver, witnesses only.
        MipProofOptions mip_options;
        mip_options.maximum_nodes = options.maximum_nodes;
        mip_options.maximum_witness_values = options.maximum_witness_values;
        mip_options.tolerance = options.tolerance;
        mip_options.relative_gap = options.relative_gap;
        mip_options.deadline = options.deadline;
        const auto mip_report = verify_mip_proof(master, proof.master_proof, mip_options);
        report.nodes_used = proof.master_proof.nodes_used;
        report.witness_values_used = proof.master_proof.witness_values_used;
        report.checked_nodes = mip_report.checked_nodes;
        report.checked_witness_values = mip_report.checked_witness_values;
        if (mip_report.status == MipProofStatus::exhausted) {
            OaBudgetKind kind = OaBudgetKind::time_limit;
            if (mip_report.exhausted_budget == MipProofBudgetKind::node_limit)
                kind = OaBudgetKind::node_limit;
            else if (mip_report.exhausted_budget == MipProofBudgetKind::witness_limit)
                kind = OaBudgetKind::witness_limit;
            return exhaust(kind, "o6 master tree: " + mip_report.message);
        }
        if (!mip_report.accepted) return fail("o6 master tree: " + mip_report.message);
        const double lower_bound = mip_report.lower_bound;
        if (proof.kind == OaProofKind::optimal && !std::isfinite(lower_bound))
            return fail("o6 master tree returned no finite bound");
        // O7 — claimed bound equals the replayed tree bound; history monotone.
        if (proof.kind == OaProofKind::optimal) {
            const double sign = oa_detail::sense_sign(source);
            const double claimed_normalized = sign * proof.claimed_best_bound;
            const double bound_tolerance = 1e-6 * std::max(1.0, std::abs(lower_bound));
            if (std::abs(claimed_normalized - lower_bound) > bound_tolerance)
                return fail("o7 claimed bound does not match the replayed master bound");
            bool seen_certified = false;
            double previous = 0.0;
            double last_certified = 0.0;
            for (const auto& record : proof.master_history) {
                const bool status_certified =
                    history_status_is_certified(record.status)[0] != '\0';
                if (record.certified && !status_certified)
                    return fail("o7 certified history entry has a non-certified status");
                if (record.certified && !std::isfinite(record.bound))
                    return fail("o7 certified history entry has a non-finite bound");
                if (record.certified) {
                    if (seen_certified && record.bound + bound_tolerance < previous)
                        return fail("o7 certified bound history is not monotone");
                    previous = record.bound;
                    last_certified = record.bound;
                    seen_certified = true;
                }
            }
            if (!seen_certified)
                return fail("o7 no certified master bound in history");
            if (std::abs(last_certified - claimed_normalized) > bound_tolerance)
                return fail("o7 claimed bound is not the last certified history bound");
        }
        if (proof.kind == OaProofKind::infeasible && !proof.master_proof.claims_infeasible)
            return fail("o10 infeasible claim without an infeasible master tree");
        // O10 — kind consistency (before O8 work on the incumbent).
        if (proof.kind == OaProofKind::infeasible &&
            (proof.claimed_objective != 0.0 || proof.claimed_best_bound != 0.0 ||
             proof.claimed_relative_gap != 0.0 || !proof.incumbent.empty()))
            return fail("o10 infeasible proof carries a claim or incumbent");
        if (proof.kind == OaProofKind::optimal && proof.incumbent.empty())
            return fail("o10 optimal proof has no incumbent");
        if (proof.kind == OaProofKind::optimal && proof.master_proof.claims_infeasible)
            return fail("o10 optimal proof embeds an infeasible master tree");
        if (proof.kind == OaProofKind::optimal) {
            // O8 — incumbent verified against the source.
            if (proof.incumbent.size() != source.matrix.column_count)
                return fail("o8 incumbent dimension mismatch");
            for (std::size_t j = 0; j < source.variable_type.size(); ++j) {
                if (source.variable_type[j] != model::VariableType::continuous &&
                    std::abs(proof.incumbent[j] - std::round(proof.incumbent[j])) >
                        options.feasibility_tolerance)
                    return fail("o8 incumbent is not integral");
            }
            const auto nlp = io::make_nlp_model(source);
            const auto feasibility = nlp::verify_nlp_feasibility(
                nlp, proof.incumbent, options.feasibility_tolerance);
            if (!feasibility.feasible)
                return fail("o8 incumbent fails source feasibility: " + feasibility.message);
            const double normalized_objective = nlp.eval_objective(proof.incumbent);
            const double claimed_normalized = oa_detail::sense_sign(source) *
                                              proof.claimed_objective;
            if (!std::isfinite(normalized_objective) ||
                std::abs(normalized_objective - claimed_normalized) >
                    options.feasibility_tolerance * std::max(1.0, std::abs(claimed_normalized)))
                return fail("o8 incumbent objective does not match the claim");
            // O9 — replayed gap inside the locked tolerance.
            const double objective_normalized = claimed_normalized;
            const double gap = std::max(0.0, objective_normalized - lower_bound) /
                               std::max(1.0, std::abs(objective_normalized));
            if (gap > 1e-3)
                return fail("o9 replayed gap exceeds the locked tolerance");
            if (gap > proof.claimed_relative_gap + 1e-9)
                return fail("o9 replayed gap exceeds the claimed gap");
            if (lower_bound > objective_normalized +
                                  options.feasibility_tolerance *
                                      std::max(1.0, std::abs(objective_normalized)))
                return fail("o9 replayed bound exceeds the incumbent");
            report.relative_gap = gap;
        }
        report.lower_bound = proof.kind == OaProofKind::optimal ? lower_bound : 0.0;
        // O11 — remaining caps.
        if (proof.cuts.size() > options.maximum_tangents)
            return exhaust(OaBudgetKind::tangent_limit, "o11 tangent budget exceeded");
        if (proof.master_proof.nodes_used > options.maximum_nodes)
            return exhaust(OaBudgetKind::node_limit, "o11 master node budget exceeded");
        if (expired()) return exhaust(OaBudgetKind::time_limit, "o11 replay deadline reached");
        // Accepted (O12: all numbers above were recomputed; no solve ran).
        report.accepted = true;
        report.status = MipProofStatus::accepted;
        report.tier = fingerprint_bound ? OaAssuranceTier::independent_oa
                                        : OaAssuranceTier::replayed_oa;
        report.checked_cuts = proof.cuts.size();
        report.message = "oa proof accepted: " + std::to_string(proof.cuts.size()) +
                         " cuts, " + std::to_string(mip_report.checked_nodes) +
                         " master tree nodes checked";
        return finish(report);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::length_error&) {
        throw;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find("deadline") != std::string::npos)
            return exhaust(OaBudgetKind::time_limit, std::string("o11 ") + error.what());
        return fail(std::string("o12 ") + error.what());
    } catch (const std::exception& error) {
        return fail(std::string("o1 ") + error.what());
    }
}
} // namespace markov_cero::verify
