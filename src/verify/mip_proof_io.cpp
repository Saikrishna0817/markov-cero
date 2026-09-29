#include "markov_cero/verify/mip_proof.hpp"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <string>
namespace markov_cero::verify {
void record_cut_obligation(MipProof& proof, std::size_t node,
    std::vector<double> coefficients, double rhs, double observed_lhs) {
    if (!std::isfinite(rhs) || !std::isfinite(observed_lhs) || observed_lhs >= rhs ||
        coefficients.empty()) throw std::invalid_argument("cut obligation needs a violated finite row");
    for (double coefficient : coefficients)
        if (!std::isfinite(coefficient)) throw std::invalid_argument("nonfinite cut coefficient");
    MipObligation note;
    note.kind = MipObligationKind::cut;
    note.node = node;
    note.coefficients = std::move(coefficients);
    note.rhs = rhs;
    note.observed_lhs = observed_lhs;
    proof.obligations.push_back(std::move(note));
}
void record_propagation_obligation(MipProof& proof, std::size_t node,
    std::size_t source_row, std::size_t variable, double source_coefficient,
    double source_rhs, double derived_bound, bool source_is_lower) {
    if (!std::isfinite(source_coefficient) || source_coefficient == 0 ||
        !std::isfinite(source_rhs) || !std::isfinite(derived_bound) ||
        std::abs(derived_bound - source_rhs / source_coefficient) >
            1e-9 * (1 + std::abs(derived_bound)))
        throw std::invalid_argument("invalid singleton propagation derivation");
    MipObligation note;
    note.kind = MipObligationKind::propagation;
    note.node = node;
    note.source_row = source_row;
    note.variable = variable;
    note.source_coefficient = source_coefficient;
    note.source_rhs = source_rhs;
    note.derived_bound = derived_bound;
    note.source_is_lower = source_is_lower;
    proof.obligations.push_back(std::move(note));
}
void write_mip_proof(std::ostream& out, const MipProof& proof) {
    if (proof.format_version != kMipProofFormatVersion)
        throw std::invalid_argument("unsupported MIP proof format version " +
            std::to_string(proof.format_version));
    if (proof.model_fingerprint.size() > kMipProofFingerprintLimit)
        throw std::invalid_argument("model fingerprint exceeds proof format limit");
    out << std::setprecision(17) << "MARKOV_MIP_PROOF " << kMipProofFormatVersion << '\n'
        << proof.claims_infeasible << ' ' << proof.objective << ' ' << proof.nodes.size() << '\n';
    const auto vector = [&](const auto& values) {
        out << values.size(); for (auto x : values) out << ' ' << x; out << '\n';
    };
    out << proof.nodes_used << ' ' << proof.witness_values_used << ' ' << proof.budget_time_ms
        << ' ' << (proof.budget_exhausted ? 1 : 0) << ' '
        << static_cast<int>(proof.exhausted_budget) << '\n';
    out << proof.model_fingerprint.size() << ' ';
    out.write(proof.model_fingerprint.data(),
        static_cast<std::streamsize>(proof.model_fingerprint.size()));
    out << '\n';
    vector(proof.incumbent);
    for (const auto& node : proof.nodes) {
        out << static_cast<int>(node.kind) << ' ' << node.variable << ' ' << node.split_value
            << ' ' << node.down << ' ' << node.up << ' ' << static_cast<int>(node.relaxation.status)
            << ' ' << node.relaxation.objective << '\n';
        vector(node.relaxation.primal); vector(node.relaxation.dual); vector(node.relaxation.certificate);
    }
    out << proof.obligations.size() << '\n';
    for (const auto& note : proof.obligations) {
        out << static_cast<int>(note.kind) << ' ' << note.node << ' ' << note.source_row
            << ' ' << note.variable << ' ' << note.rhs << ' ' << note.observed_lhs
            << ' ' << note.source_coefficient << ' ' << note.source_rhs << ' '
            << note.derived_bound << ' ' << note.source_is_lower << '\n';
        vector(note.coefficients);
    }
    if (!out) throw std::runtime_error("cannot write MIP proof");
}
MipProof read_mip_proof(std::istream& input, const MipProofOptions& limits) {
    const auto expired = [&] {
        return limits.deadline && std::chrono::steady_clock::now() >= *limits.deadline;
    };
    MipProof proof;
    std::string magic; int version; std::size_t count, total = 0;
    if (!(input >> std::setw(32) >> magic >> version) || magic != "MARKOV_MIP_PROOF")
        throw std::invalid_argument("invalid proof header or node budget");
    if (version != static_cast<int>(kMipProofFormatVersion))
        throw std::invalid_argument("unsupported MIP proof format version " + std::to_string(version) +
            " (expected " + std::to_string(kMipProofFormatVersion) + ")");
    if (!(input >> proof.claims_infeasible >> proof.objective >> count) || count > limits.maximum_nodes ||
        !std::isfinite(proof.objective)) throw std::invalid_argument("invalid proof header or node budget");
    proof.format_version = kMipProofFormatVersion;
    int exhausted, budget;
    if (!(input >> proof.nodes_used >> proof.witness_values_used >> proof.budget_time_ms >>
            exhausted >> budget) ||
        exhausted < 0 || exhausted > 1 || budget < 0 ||
        budget > static_cast<int>(MipProofBudgetKind::witness_limit) ||
        !std::isfinite(proof.budget_time_ms) || proof.budget_time_ms < 0 ||
        (exhausted != 0) != (budget != static_cast<int>(MipProofBudgetKind::none)))
        throw std::invalid_argument("invalid proof budget record");
    proof.budget_exhausted = exhausted != 0;
    proof.exhausted_budget = static_cast<MipProofBudgetKind>(budget);
    std::size_t fingerprint_size;
    if (!(input >> fingerprint_size) || fingerprint_size > kMipProofFingerprintLimit)
        throw std::invalid_argument("invalid proof fingerprint");
    char separator;
    if (!input.get(separator)) throw std::invalid_argument("invalid proof fingerprint");
    proof.model_fingerprint.assign(fingerprint_size, '\0');
    if (fingerprint_size > 0 &&
        !input.read(proof.model_fingerprint.data(), static_cast<std::streamsize>(fingerprint_size)))
        throw std::invalid_argument("invalid proof fingerprint");
    if (expired()) throw std::runtime_error("proof parsing deadline");
    const auto vector = [&](std::vector<double>& values) {
        std::size_t size;
        if (!(input >> size) || size > limits.maximum_witness_values - total)
            throw std::invalid_argument("proof vector budget");
        total += size; values.resize(size);
        std::size_t scanned = 0;
        for (auto& value : values) {
            if ((scanned++ & 1023U) == 0 && expired())
                throw std::runtime_error("proof parsing deadline");
            if (!(input >> value) || !std::isfinite(value)) throw std::invalid_argument("invalid proof scalar");
        }
    };
    vector(proof.incumbent); proof.nodes.resize(count);
    for (auto& node : proof.nodes) {
        if (expired()) throw std::runtime_error("proof parsing deadline");
        int kind, status;
        if (!(input >> kind >> node.variable >> node.split_value >> node.down >> node.up >> status
                    >> node.relaxation.objective) || kind < 0 || kind > 4 || status < 0 || status > 12 ||
            !std::isfinite(node.split_value) || !std::isfinite(node.relaxation.objective))
            throw std::invalid_argument("invalid proof node");
        node.kind = static_cast<MipProofKind>(kind);
        node.relaxation.status = static_cast<lp::reference::SolveStatus>(status);
        vector(node.relaxation.primal); vector(node.relaxation.dual); vector(node.relaxation.certificate);
    }
    input >> std::ws;
    if (input.eof()) return proof; // A stripped audit trailer is valid.
    std::size_t annotations;
    if (!(input >> annotations) || annotations > limits.maximum_witness_values - total)
        throw std::invalid_argument("invalid proof annotation count");
    proof.obligations.resize(annotations);
    for (auto& note : proof.obligations) {
        int kind, source_lower;
        if (!(input >> kind >> note.node >> note.source_row >> note.variable >> note.rhs
              >> note.observed_lhs >> note.source_coefficient >> note.source_rhs
              >> note.derived_bound >> source_lower) || kind < 0 || kind > 1 ||
            source_lower < 0 || source_lower > 1 ||
            !std::isfinite(note.rhs) || !std::isfinite(note.observed_lhs) ||
            !std::isfinite(note.source_coefficient) || !std::isfinite(note.source_rhs) ||
            !std::isfinite(note.derived_bound))
            throw std::invalid_argument("invalid proof annotation");
        note.kind = static_cast<MipObligationKind>(kind);
        note.source_is_lower = source_lower != 0;
        vector(note.coefficients);
    }
    input >> std::ws;
    if (!input.eof()) throw std::invalid_argument("trailing proof data");
    return proof;
}
} // namespace markov_cero::verify
