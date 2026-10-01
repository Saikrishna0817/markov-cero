// MINLP-02 (docs/contracts/minlp-proof-replay.md §6.3): standalone replay of
// an exported OA proof record against its source model. Source-blind parse,
// source-bound verification: exit 0 accepted, 1 rejected (including malformed
// artifacts and exhaustion), 2 usage.
#include "cli_usage.hpp"
#include "mip_verify_options.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/verify/oa_proof.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) { markov_cero::apps::minlp_verify_usage(std::cerr); return 2; }
    try {
        markov_cero::verify::MipProofOptions mip_limits;
        double seconds = 5.0;
        for (int i = 3; i < argc; ++i) {
            if (!markov_cero::apps::parse_mip_verify_options(argc, argv, i, mip_limits, seconds)) {
                markov_cero::apps::minlp_verify_usage(std::cerr);
                return 2;
            }
        }
        markov_cero::verify::OaProofOptions limits;
        limits.maximum_nodes = mip_limits.maximum_nodes;
        limits.maximum_witness_values = mip_limits.maximum_witness_values;
        limits.relative_gap = mip_limits.relative_gap;
        std::ifstream model_file(argv[1]), proof_file(argv[2]);
        if (!model_file || !proof_file) {
            std::cerr << "cannot open model or proof\n";
            return 2;
        }
        const auto model = markov_cero::io::parse_mps(model_file);
        limits.deadline = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(seconds));
        try {
            const auto proof = markov_cero::verify::read_oa_proof(proof_file, limits);
            const auto report = markov_cero::verify::verify_oa_proof(model, proof, limits);
            std::cout << (report.accepted ? "VERIFIED: " : "REJECTED: ") << report.message
                      << '\n';
            return report.accepted ? 0 : 1;
        } catch (const std::exception& error) {
            // Contract §6.3: a malformed or budget-infeasible artifact is a
            // rejection of that artifact, not a usage error.
            std::cout << "REJECTED: " << error.what() << '\n';
            return 1;
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
