#pragma once
#include "markov_cero/model/model.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include <chrono>
#include <optional>
#include <iosfwd>

namespace markov_cero::verify {
enum class MipProofKind { open, split, bound, infeasible, empty_domain };
struct MipProofNode {
    MipProofKind kind{MipProofKind::open};
    std::size_t variable{}, down{}, up{};
    double split_value{};
    lp::reference::Result relaxation;
};
struct MipProof {
    std::vector<MipProofNode> nodes;
    std::vector<double> incumbent;
    double objective{};
    bool claims_infeasible{false};
};
struct MipProofOptions {
    std::size_t maximum_nodes{10000};
    std::size_t maximum_witness_values{4000000};
    double tolerance{1e-8};
    double relative_gap{0};
    std::optional<std::chrono::steady_clock::time_point> deadline;
};
struct MipProofReport {
    bool accepted{false};
    std::size_t checked_nodes{};
    double lower_bound{-std::numeric_limits<double>::infinity()};
    std::string message;
};
void write_mip_proof(std::ostream&, const MipProof&);
[[nodiscard]] MipProof read_mip_proof(std::istream&, const MipProofOptions& = {});
// Generates a cut-free, original-domain tree. The checker never calls a solver.
[[nodiscard]] MipProof build_mip_proof(const model::Model&, const std::vector<double>&,
    double objective, bool infeasible, const MipProofOptions& = {});
[[nodiscard]] MipProofReport verify_mip_proof(const model::Model&, const MipProof&,
    const MipProofOptions& = {});
} // namespace markov_cero::verify
