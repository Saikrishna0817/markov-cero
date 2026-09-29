// External consumer built against an installed markov-cero prefix only.
// Solves a model, exports the independent MIP proof and replays it.
#include "markov_cero/api/solve.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/verify/mip_proof.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: installed-consumer MODEL.mps\n";
        return 2;
    }
    std::ifstream model_file(argv[1]);
    if (!model_file) {
        std::cerr << "cannot open model file\n";
        return 2;
    }
    const auto model = markov_cero::io::parse_mps(model_file);

    markov_cero::api::SolveOptions options;
    options.engine = "milp";
    options.enable_mip_proof = true;
    const auto result = markov_cero::api::solve_model(model, options);
    std::cout << "status=" << markov_cero::lp::reference::to_string(result.status)
              << " verified=" << (result.verified ? "yes" : "no")
              << " certificate=" << result.certificate_type
              << " objective=" << result.objective
              << " fingerprint=" << result.model_fingerprint << '\n';
    if (!result.verified || !result.mip_proof) {
        std::cerr << "solve produced no independently verifiable proof\n";
        return 1;
    }

    std::ostringstream serialized;
    markov_cero::verify::write_mip_proof(serialized, *result.mip_proof);
    const std::string proof_text = serialized.str();
    std::ofstream proof_file("proof.txt");
    proof_file << proof_text;
    proof_file.close();

    std::istringstream replay_source(proof_text);
    const auto proof = markov_cero::verify::read_mip_proof(replay_source);
    const auto report = markov_cero::verify::verify_mip_proof(model, proof);
    std::cout << "replay accepted=" << (report.accepted ? "yes" : "no")
              << " checked_nodes=" << report.checked_nodes
              << " message=" << report.message << '\n';
    if (!report.accepted) return 1;
    std::cout << "installed-consumer PASS\n";
    return 0;
}
