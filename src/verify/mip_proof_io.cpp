#include "markov_cero/verify/mip_proof.hpp"
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <cmath>
namespace markov_cero::verify {
void write_mip_proof(std::ostream& out, const MipProof& proof) {
    out << std::setprecision(17) << "MARKOV_MIP_PROOF 1\n" << proof.claims_infeasible
        << ' ' << proof.objective << ' ' << proof.nodes.size() << '\n';
    const auto vector = [&](const auto& values) {
        out << values.size(); for (auto x : values) out << ' ' << x; out << '\n';
    };
    vector(proof.incumbent);
    for (const auto& node : proof.nodes) {
        out << static_cast<int>(node.kind) << ' ' << node.variable << ' ' << node.split_value
            << ' ' << node.down << ' ' << node.up << ' ' << static_cast<int>(node.relaxation.status)
            << ' ' << node.relaxation.objective << '\n';
        vector(node.relaxation.primal); vector(node.relaxation.dual); vector(node.relaxation.certificate);
    }
    if (!out) throw std::runtime_error("cannot write MIP proof");
}
MipProof read_mip_proof(std::istream& input, const MipProofOptions& limits) {
    MipProof proof;
    std::string magic; int version; std::size_t count, total = 0;
    if (!(input >> std::setw(32) >> magic >> version) || magic != "MARKOV_MIP_PROOF" || version != 1 ||
        !(input >> proof.claims_infeasible >> proof.objective >> count) || count > limits.maximum_nodes ||
        !std::isfinite(proof.objective)) throw std::invalid_argument("invalid proof header or node budget");
    const auto vector = [&](std::vector<double>& values) {
        std::size_t size;
        if (!(input >> size) || size > limits.maximum_witness_values - total)
            throw std::invalid_argument("proof vector budget");
        total += size; values.resize(size);
        for (auto& value : values)
            if (!(input >> value) || !std::isfinite(value)) throw std::invalid_argument("invalid proof scalar");
    };
    vector(proof.incumbent); proof.nodes.resize(count);
    for (auto& node : proof.nodes) {
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
    if (!input.eof()) throw std::invalid_argument("trailing proof data");
    return proof;
}
} // namespace markov_cero::verify
