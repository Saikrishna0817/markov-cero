// Contract v1 assurance-label derivation (docs/contracts/numerical-policy.md).
// Single source of truth for SolveResult::assurance: engines set status,
// certificate_type, guarantee_tier and the verification flags; this function
// maps them to the typed label. Deliberately fail-closed: anything not
// explicitly recognised as an accepted check reports "unverified".
#include "api_internal.hpp"

namespace markov_cero::api::detail {
namespace {

// Statuses for which a class-specific witness can carry a global claim.
// GapSatisfied is included because a verified bound plus incumbent is a real
// (weaker) global statement; Feasible and LocalStationary are deliberately not.
bool is_global_status(lp::reference::SolveStatus status) {
    switch (status) {
    case lp::reference::SolveStatus::optimal:
    case lp::reference::SolveStatus::infeasible:
    case lp::reference::SolveStatus::unbounded:
    case lp::reference::SolveStatus::gap_satisfied:
        return true;
    default:
        return false;
    }
}

// Certificates that stand for a class-specific witness: canonical LP dual gap,
// convex QP KKT, PDLP linear primal/dual gap. Incumbent-only and solver-trusted
// certificates are excluded by construction.
bool is_witness_certificate(const std::string& certificate) {
    return certificate == "canonical_lp_witness" || certificate == "convex_qp_kkt" ||
           certificate == "linear_primal_dual_gap";
}

} // namespace

std::string derive_assurance(const SolveResult& out) {
    // Strongest first: an accepted, bounded, independently replayed proof tree.
    const bool proof_accepted =
        out.proof_status == "accepted" &&
        (out.guarantee_tier == "independent_tree" || out.guarantee_tier == "replayed_tree");
    if (proof_accepted && out.canonical_verified) return "tree_replayed";

    // MINLP-02 (minlp-proof-replay.md §5.4): an accepted OA proof with a
    // model fingerprint, canonical only via §5.2's single path. The
    // replayed_oa tier never sets canonical_verified, so oa_replayed is
    // emitted iff an independent replay held.
    const bool oa_proof_accepted =
        out.proof_status == "accepted" &&
        (out.guarantee_tier == "independent_oa" || out.guarantee_tier == "replayed_oa");
    if (oa_proof_accepted && out.canonical_verified) return "oa_replayed";

    // NLP: a first-order KKT candidate. Never a global certificate and never a
    // proof of a local minimum; local_minimum_verified stays reserved.
    if (out.certificate_type == "local_kkt") {
        return (out.canonical_verified && out.original_verified) ? "local_kkt_checked"
                                                                 : "unverified";
    }

    // Solver-trusted MINLP OA bounds and incumbent-only results never rise
    // above the original primal check, however small the reported gap.
    const bool solver_trusted =
        out.certificate_type.find("solver_trusted_oa") != std::string::npos;
    if (!solver_trusted && is_global_status(out.status) && out.verified &&
        out.canonical_verified && is_witness_certificate(out.certificate_type)) {
        return "optimality_witness_checked";
    }

    if (out.original_verified) return "original_primal_checked";
    return "unverified";
}

} // namespace markov_cero::api::detail
