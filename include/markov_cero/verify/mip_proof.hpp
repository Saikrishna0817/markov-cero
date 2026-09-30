#pragma once
#include "markov_cero/model/model.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <iosfwd>
#include <string>
#include <vector>

namespace markov_cero::verify {
inline constexpr std::uint32_t kMipProofFormatVersion = 3;
inline constexpr std::size_t kMipProofFingerprintLimit = 4096;
enum class MipProofKind { open, split, bound, infeasible, empty_domain };
enum class MipAssuranceTier { independent_tree, replayed_tree, unverified };
enum class MipProofBudgetKind { none, time_limit, node_limit, witness_limit };
enum class MipProofStatus { accepted, exhausted, rejected, unsupported };
enum class MipObligationKind { cut, propagation };
// Audit note from an optimizer event. Replay deliberately ignores these notes:
// it verifies the original-domain, cut-free tree independently. Their
// structure is still validated at replay (domain and finiteness), so a
// tampered artifact cannot carry malformed rows (contract §7.1).
struct MipObligation {
    MipObligationKind kind{MipObligationKind::cut};
    std::size_t node{};
    std::size_t source_row{};
    std::size_t variable{};
    std::vector<double> coefficients;
    double rhs{};
    double observed_lhs{};
    double source_coefficient{};
    double source_rhs{};
    double derived_bound{};
    bool source_is_lower{false};
};
struct MipProofNode {
    MipProofKind kind{MipProofKind::open};
    std::size_t variable{}, down{}, up{};
    double split_value{};
    lp::reference::Result relaxation;
};
struct MipProof {
    std::vector<MipProofNode> nodes;
    std::vector<MipObligation> obligations;
    std::vector<double> incumbent;
    double objective{};
    bool claims_infeasible{false};
    std::uint32_t format_version{kMipProofFormatVersion};
    std::string model_fingerprint;
    std::size_t nodes_used{};
    std::size_t witness_values_used{};
    double budget_time_ms{};
    bool budget_exhausted{false};
    MipProofBudgetKind exhausted_budget{MipProofBudgetKind::none};
};
struct MipProofOptions {
    std::size_t maximum_nodes{10000};
    std::size_t maximum_witness_values{4000000};
    double tolerance{1e-8};
    double relative_gap{0};
    // Absolute proof deadline. Pass the same value to build and replay so both
    // stages consume one budget; unset permits an unbounded standalone call.
    std::optional<std::chrono::steady_clock::time_point> deadline;
};
struct MipProofReport {
    bool accepted{false};
    std::size_t checked_nodes{};
    std::size_t nodes_used{};
    double lower_bound{-std::numeric_limits<double>::infinity()};
    std::string message;
    std::uint32_t format_version{kMipProofFormatVersion};
    std::string model_fingerprint;
    MipAssuranceTier tier{MipAssuranceTier::unverified};
    MipProofStatus status{MipProofStatus::rejected};
    bool budget_exhausted{false};
    MipProofBudgetKind exhausted_budget{MipProofBudgetKind::none};
    std::size_t witness_values_used{};
    std::size_t checked_witness_values{};
    double budget_time_ms{};
    double build_ms{};
    double replay_ms{};
};
void write_mip_proof(std::ostream&, const MipProof&);
void record_cut_obligation(MipProof&, std::size_t node, std::vector<double> coefficients,
    double rhs, double observed_lhs);
void record_propagation_obligation(MipProof&, std::size_t node, std::size_t source_row,
    std::size_t variable, double source_coefficient, double source_rhs,
    double derived_bound, bool source_is_lower);
[[nodiscard]] MipProof read_mip_proof(std::istream&, const MipProofOptions& = {});
// Generates a cut-free, original-domain tree. The checker never calls a solver.
[[nodiscard]] MipProof build_mip_proof(const model::Model&, const std::vector<double>&,
    double objective, bool infeasible, const MipProofOptions& = {},
    const std::string& model_fingerprint = {},
    const std::vector<MipObligation>& audit_annotations = {});
[[nodiscard]] MipProofReport verify_mip_proof(const model::Model&, const MipProof&,
    const MipProofOptions& = {});
} // namespace markov_cero::verify
