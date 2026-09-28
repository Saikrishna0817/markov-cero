#include "markov_cero/io/mps.hpp"
#include "markov_cero/verify/mip_proof.hpp"
#include <fstream>
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: markov-cero-verify-mip MODEL.mps PROOF.txt\n"; return 2; }
    try {
        std::ifstream model_file(argv[1]), proof_file(argv[2]);
        if (!model_file || !proof_file) throw std::runtime_error("cannot open model or proof");
        const auto model = markov_cero::io::parse_mps(model_file);
        const auto proof = markov_cero::verify::read_mip_proof(proof_file);
        const auto report = markov_cero::verify::verify_mip_proof(model, proof);
        std::cout << (report.accepted ? "VERIFIED: " : "REJECTED: ") << report.message << '\n';
        return report.accepted ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
