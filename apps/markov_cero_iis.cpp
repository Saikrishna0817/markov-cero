#include "markov_cero/analysis/iis_analyzer.hpp"
#include "markov_cero/io/mps.hpp"
#include "markov_cero/io/lp_parser.hpp"
#include <fstream>
#include <iostream>
int main(int argc, char** argv) {
    try {
        if (argc != 2) { std::cerr << "usage: markov-cero-iis MODEL.mps|MODEL.lp\n"; return 2; }
        const std::string path = argv[1];
        std::ifstream input(path);
        if (!input) throw std::runtime_error("cannot open model");
        const auto model = path.ends_with(".lp") || path.ends_with(".LP")
            ? markov_cero::io::parse_lp_file(path) : markov_cero::io::parse_mps(input);
        const auto result = markov_cero::analysis::compute_iis(model);
        std::cout << result.diagnostic_summary << '\n';
        return result.is_infeasible && result.complete ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
