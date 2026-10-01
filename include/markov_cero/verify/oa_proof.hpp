#pragma once

// MINLP-02 (docs/contracts/minlp-proof-replay.md): versioned outer-
// approximation proof record for restricted convex quadratic MINLPs and its
// deterministic replay. Build records source fingerprint, convexity
// evidence, every tangent, the master revision history, the source-sense
// claim, the incumbent, and an embedded cut-free MIP tree proof of the
// rebuilt OA master. Replay re-derives everything from the source model and
// never calls a solver (contract §3).

#include "markov_cero/minlp/minlp_solver.hpp"
#include "markov_cero/verify/mip_proof.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace markov_cero::verify {

// Strict-equality version checked by writer, reader and verifier, pinned by
// tests (contract §11.2).
inline constexpr std::uint32_t kOaProofFormatVersion = 1;
inline constexpr std::size_t kOaProofFingerprintLimit = 4096;

enum class OaProofKind { optimal, infeasible };
enum class OaProofSense { minimize, maximize };
enum class OaAssuranceTier { independent_oa, replayed_oa, unverified };
// Budget taxonomy for the wrapper record; the embedded MIP block keeps
// MipProofBudgetKind (contract §11.1).
enum class OaBudgetKind { none, time_limit, node_limit, witness_limit, tangent_limit };

// Builder-side budget refusal: maps to proof_status "exhausted" with
// kind tangent_limit (contract §4.3, O11).
struct OaBudgetExhausted final : std::runtime_error {
    explicit OaBudgetExhausted(const std::string& what) : std::runtime_error(what) {}
};

struct OaProof {
    std::uint32_t format_version{kOaProofFormatVersion};
    std::string model_fingerprint;
    OaProofSense source_sense{OaProofSense::minimize};
    OaProofKind kind{OaProofKind::optimal};
    std::vector<double> convexity_pivots;
    std::vector<minlp::OaCut> cuts;
    std::vector<minlp::MasterRecord> master_history;
    double claimed_objective{0.0};
    double claimed_best_bound{0.0};
    double claimed_relative_gap{0.0};
    std::vector<double> incumbent;
    MipProof master_proof;
};

struct OaProofOptions {
    std::size_t maximum_nodes{10000};
    std::size_t maximum_witness_values{4000000};
    std::size_t maximum_tangents{10000};
    double tolerance{minlp::kOaCutReplayTolerance};
    double feasibility_tolerance{1e-6};
    double relative_gap{0.0};
    std::optional<std::chrono::steady_clock::time_point> deadline;
};

struct OaProofReport {
    bool accepted{false};
    MipProofStatus status{MipProofStatus::rejected};
    OaAssuranceTier tier{OaAssuranceTier::unverified};
    OaBudgetKind exhausted_budget{OaBudgetKind::none};
    bool budget_exhausted{false};
    std::string message;
    std::uint32_t format_version{0};
    std::string model_fingerprint;
    std::size_t checked_cuts{0};
    std::size_t nodes_used{0};
    std::size_t checked_nodes{0};
    std::size_t witness_values_used{0};
    std::size_t checked_witness_values{0};
    // Certified lower bound (normalized minimization sense) from O6 and the
    // replayed relative gap of O9; both finite iff accepted.
    double lower_bound{0.0};
    double relative_gap{0.0};
    double replay_ms{0.0};
};

void write_oa_proof(std::ostream&, const OaProof&);

// Source-blind structural parse; semantic checks live in verify_oa_proof.
// Throws std::invalid_argument / std::runtime_error on deadline.
[[nodiscard]] OaProof read_oa_proof(std::istream&, const OaProofOptions& = {});

// Generator side (contract §4.3): re-derives cuts from the source, rebuilds
// the master with the verifier-side assembly and embeds build_mip_proof of
// that master. Throws OaBudgetExhausted on the tangent cap, std::
// invalid_argument on a missing master or failed convexity evidence.
[[nodiscard]] OaProof build_oa_proof(const model::Model& source,
                                     const minlp::MinlpSolution& solution,
                                     const OaProofOptions& options,
                                     const std::string& model_fingerprint);

// Deterministic replay (contract §3, obligations O1-O12). No optimization.
[[nodiscard]] OaProofReport verify_oa_proof(const model::Model& source,
                                             const OaProof& proof,
                                             const OaProofOptions& options = {});

} // namespace markov_cero::verify
