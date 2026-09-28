#include "cli_usage.hpp"
#include "mip_verify_options.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/verify/mip_proof.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) { markov_cero::apps::mip_verify_usage(std::cerr); return 2; }
    try {
        markov_cero::verify::MipProofOptions limits;
        double seconds = 5.0;
        for (int i = 3; i < argc; ++i) {
            if (!markov_cero::apps::parse_mip_verify_options(argc, argv, i, limits, seconds)) {
                markov_cero::apps::mip_verify_usage(std::cerr);
                return 2;
            }
        }
        std::ifstream model_file(argv[1]), proof_file(argv[2]);
        if (!model_file || !proof_file) throw std::runtime_error("cannot open model or proof");
        const auto model = markov_cero::io::parse_mps(model_file);
        limits.deadline = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(seconds));
        const auto proof = markov_cero::verify::read_mip_proof(proof_file, limits);
        const auto report = markov_cero::verify::verify_mip_proof(model, proof, limits);
        std::cout << (report.accepted ? "VERIFIED: " : "REJECTED: ") << report.message << '\n';
        return report.accepted ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
