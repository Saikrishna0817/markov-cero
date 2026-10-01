// MINLP-02 (docs/contracts/minlp-proof-replay.md §2.2): whitespace-token
// serialization of the OA proof record with a length-prefixed embedded MIP
// block. Reader is source-blind and fail-closed: strict version equality,
// finite scalars, range-checked tags, vector budgets, deadline polling and
// trailing-data rejection.
#include "markov_cero/verify/oa_proof.hpp"

#include <chrono>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace markov_cero::verify {
namespace {
int kind_tag(OaProofKind kind) { return kind == OaProofKind::infeasible ? 1 : 0; }
int sense_tag(OaProofSense sense) { return sense == OaProofSense::maximize ? 1 : 0; }
} // namespace

void write_oa_proof(std::ostream& out, const OaProof& proof) {
    if (proof.format_version != kOaProofFormatVersion)
        throw std::invalid_argument("unsupported OA proof format version " +
                                    std::to_string(proof.format_version));
    if (proof.model_fingerprint.size() > kOaProofFingerprintLimit)
        throw std::invalid_argument("model fingerprint exceeds OA proof format limit");
    out << std::setprecision(17) << "MARKOV_OA_PROOF " << kOaProofFormatVersion << '\n'
        << kind_tag(proof.kind) << ' ' << sense_tag(proof.source_sense) << '\n';
    out << proof.model_fingerprint.size() << ' ';
    out.write(proof.model_fingerprint.data(),
              static_cast<std::streamsize>(proof.model_fingerprint.size()));
    out << '\n';
    out << proof.claimed_objective << ' ' << proof.claimed_best_bound << ' '
        << proof.claimed_relative_gap << '\n';
    out << proof.convexity_pivots.size();
    for (double value : proof.convexity_pivots) out << ' ' << value;
    out << '\n';
    out << proof.cuts.size() << '\n';
    for (const auto& cut : proof.cuts) {
        out << static_cast<int>(cut.source_kind) << ' ' << cut.source_index << ' '
            << (cut.source_maximize ? 1 : 0);
        out << ' ' << cut.point.size();
        for (double value : cut.point) out << ' ' << value;
        out << ' ' << cut.gradient.size();
        for (double value : cut.gradient) out << ' ' << value;
        out << ' ' << cut.value << ' ' << cut.rhs << ' ' << cut.weakening << '\n';
    }
    out << proof.master_history.size() << '\n';
    for (const auto& record : proof.master_history) {
        // §2.1/O7: history entries serialize finite bounds only; a master
        // without a finite bound is disclosed by status + certified=false.
        const double bound = std::isfinite(record.bound) ? record.bound : 0.0;
        out << record.iteration << ' ' << static_cast<int>(record.status) << ' '
            << (record.certified ? 1 : 0) << ' ' << bound << ' ' << record.cut_count
            << '\n';
    }
    out << proof.incumbent.size();
    for (double value : proof.incumbent) out << ' ' << value;
    out << '\n';
    std::ostringstream embedded;
    write_mip_proof(embedded, proof.master_proof);
    const std::string bytes = embedded.str();
    out << bytes.size() << '\n';
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("cannot write OA proof");
}

OaProof read_oa_proof(std::istream& input, const OaProofOptions& limits) {
    const auto expired = [&] {
        return limits.deadline && std::chrono::steady_clock::now() >= *limits.deadline;
    };
    OaProof proof;
    std::string magic;
    int version = 0;
    if (!(input >> std::setw(32) >> magic >> version) || magic != "MARKOV_OA_PROOF")
        throw std::invalid_argument("invalid OA proof header");
    if (version != static_cast<int>(kOaProofFormatVersion))
        throw std::invalid_argument("unsupported OA proof format version " +
                                    std::to_string(version) + " (expected " +
                                    std::to_string(kOaProofFormatVersion) + ")");
    proof.format_version = kOaProofFormatVersion;
    int kind = -1, sense = -1;
    if (!(input >> kind >> sense) || (kind != 0 && kind != 1) || (sense != 0 && sense != 1))
        throw std::invalid_argument("invalid OA proof kind or sense");
    proof.kind = kind == 1 ? OaProofKind::infeasible : OaProofKind::optimal;
    proof.source_sense = sense == 1 ? OaProofSense::maximize : OaProofSense::minimize;
    std::size_t fingerprint_size = 0;
    if (!(input >> fingerprint_size) || fingerprint_size > kOaProofFingerprintLimit)
        throw std::invalid_argument("invalid OA proof fingerprint");
    char separator = '\0';
    if (!input.get(separator)) throw std::invalid_argument("invalid OA proof fingerprint");
    proof.model_fingerprint.assign(fingerprint_size, '\0');
    if (fingerprint_size > 0 &&
        !input.read(proof.model_fingerprint.data(),
                    static_cast<std::streamsize>(fingerprint_size)))
        throw std::invalid_argument("invalid OA proof fingerprint");
    std::size_t scalar_total = 0;
    const auto read_scalars = [&](std::vector<double>& values) {
        for (auto& value : values) {
            if ((scalar_total++ & 1023U) == 0 && expired())
                throw std::runtime_error("OA proof parsing deadline");
            if (!(input >> value) || !std::isfinite(value))
                throw std::invalid_argument("invalid OA proof scalar");
        }
    };
    if (!(input >> proof.claimed_objective >> proof.claimed_best_bound >>
          proof.claimed_relative_gap) ||
        !std::isfinite(proof.claimed_objective) || !std::isfinite(proof.claimed_best_bound) ||
        !std::isfinite(proof.claimed_relative_gap))
        throw std::invalid_argument("invalid OA proof claim");
    std::size_t count = 0;
    if (!(input >> count) || count > 4096U) throw std::invalid_argument("invalid OA pivot count");
    proof.convexity_pivots.resize(count);
    read_scalars(proof.convexity_pivots);
    if (!(input >> count) || count > limits.maximum_tangents)
        throw std::invalid_argument("OA proof tangent budget");
    proof.cuts.resize(count);
    for (auto& cut : proof.cuts) {
        if (expired()) throw std::runtime_error("OA proof parsing deadline");
        int cut_kind = -1, maximize = -1;
        std::size_t point_size = 0;
        if (!(input >> cut_kind >> cut.source_index >> maximize >> point_size) || cut_kind < 0 ||
            cut_kind > 3 || (maximize != 0 && maximize != 1) || point_size > 4096U ||
            scalar_total + point_size + 3 > limits.maximum_witness_values)
            throw std::invalid_argument("invalid OA proof cut header");
        cut.source_kind = static_cast<minlp::OaCutSource>(cut_kind);
        cut.source_maximize = maximize != 0;
        cut.point.resize(point_size);
        read_scalars(cut.point);
        std::size_t gradient_size = 0;
        if (!(input >> gradient_size) || gradient_size > 4096U ||
            scalar_total + gradient_size + 3 > limits.maximum_witness_values)
            throw std::invalid_argument("invalid OA proof cut gradient header");
        cut.gradient.resize(gradient_size);
        read_scalars(cut.gradient);
        if (!(input >> cut.value >> cut.rhs >> cut.weakening) || !std::isfinite(cut.value) ||
            !std::isfinite(cut.rhs) || !std::isfinite(cut.weakening))
            throw std::invalid_argument("invalid OA proof cut scalar");
        scalar_total += 3;
    }
    if (!(input >> count) || count > limits.maximum_tangents)
        throw std::invalid_argument("OA proof history budget");
    proof.master_history.resize(count);
    for (auto& record : proof.master_history) {
        int status = -1, certified = -1;
        if (!(input >> record.iteration >> status >> certified >> record.bound >>
              record.cut_count) ||
            status < 0 || status > 12 || (certified != 0 && certified != 1) ||
            !std::isfinite(record.bound))
            throw std::invalid_argument("invalid OA proof history record");
        record.status = static_cast<lp::reference::SolveStatus>(status);
        record.certified = certified != 0;
    }
    if (!(input >> count) || count > limits.maximum_witness_values - scalar_total)
        throw std::invalid_argument("OA proof incumbent budget");
    proof.incumbent.resize(count);
    read_scalars(proof.incumbent);
    // Infeasible proofs carry no claim (contract §2.1).
    if (proof.kind == OaProofKind::infeasible &&
        (proof.claimed_objective != 0.0 || proof.claimed_best_bound != 0.0 ||
         proof.claimed_relative_gap != 0.0 || !proof.incumbent.empty()))
        throw std::invalid_argument("infeasible OA proof carries a claim");
    if (proof.kind == OaProofKind::optimal && proof.incumbent.empty())
        throw std::invalid_argument("optimal OA proof has no incumbent");
    if (expired()) throw std::runtime_error("OA proof parsing deadline");
    std::size_t byte_len = 0;
    if (!(input >> byte_len) || byte_len > 1ULL << 30)
        throw std::invalid_argument("invalid OA proof embedded block length");
    char newline = '\0';
    if (!input.get(newline)) throw std::invalid_argument("invalid OA proof embedded block");
    std::string bytes(byte_len, '\0');
    if (byte_len > 0 &&
        !input.read(bytes.data(), static_cast<std::streamsize>(byte_len)))
        throw std::invalid_argument("truncated OA proof embedded block");
    std::istringstream embedded(bytes);
    proof.master_proof = read_mip_proof(embedded, [&] {
        MipProofOptions mip;
        mip.maximum_nodes = limits.maximum_nodes;
        mip.maximum_witness_values = limits.maximum_witness_values;
        mip.relative_gap = limits.relative_gap;
        mip.deadline = limits.deadline;
        return mip;
    }());
    input >> std::ws;
    if (!input.eof()) throw std::invalid_argument("trailing OA proof data");
    return proof;
}
} // namespace markov_cero::verify
