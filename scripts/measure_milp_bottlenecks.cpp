#include "markov_cero/io/mps.hpp"
#include "markov_cero/milp/milp_solver.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc < 2) throw std::invalid_argument("supply at least one MPS path");
    std::cout << "instance,status,nodes,lp_iterations,cuts,heuristics,lp_bound_ms,"
                 "incumbent_ms,search_ms,runtime_ms\n";
    for (int index = 1; index < argc; ++index) {
        std::ifstream input(argv[index]);
        if (!input) throw std::runtime_error("cannot open MPS instance");
        const auto model = markov_cero::io::parse_mps(input);
        markov_cero::milp::Options options;
        options.time_limit_seconds = 8.0;
        options.max_nodes = 2000;
        options.enable_strong_branching = false;
        const auto result = markov_cero::milp::solve(model, options);
        std::cout << std::setprecision(9) << argv[index] << ','
                  << static_cast<int>(result.status) << ',' << result.nodes_explored << ','
                  << result.lp_iterations << ',' << result.cuts_generated << ','
                  << result.heuristics_found << ',' << result.lp_bound_ms << ','
                  << result.incumbent_ms << ',' << result.search_ms << ','
                  << result.runtime_ms << '\n';
    }
}
