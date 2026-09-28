#pragma once
#include "cli_numbers.hpp"
#include <cstddef>
#include <iostream>
#include <string>

namespace markov_cero::apps {
inline int parse_solve_time_option(const std::string& arg, int argc, char** argv, int& index,
                                   double& seconds, double& milp_seconds, double& lp_seconds) {
    if (arg != "--time-limit") return 0;
    if (index + 1 >= argc || !parse_nonnegative(argv[++index], seconds) ||
        seconds <= 0 || seconds > 1e8) {
        std::cerr << "invalid time limit: expected 0 < seconds <= 1e8\n";
        return -1;
    }
    milp_seconds = lp_seconds = seconds;
    return 1;
}
inline int parse_proof_budget_option(const std::string& arg, int argc, char** argv, int& index,
                                    double& seconds, std::size_t& nodes,
                                    std::size_t& witness_values) {
    if (arg == "--proof-time-limit") {
        if (index + 1 >= argc || !parse_nonnegative(argv[++index], seconds) ||
            seconds <= 0 || seconds > 1e8) {
            std::cerr << "invalid proof time limit: expected 0 < seconds <= 1e8\n";
            return -1;
        }
        return 1;
    }
    if (arg != "--proof-max-nodes" && arg != "--proof-max-values") return 0;
    if (index + 1 >= argc) {
        std::cerr << "missing MIP proof budget value\n";
        return -1;
    }
    auto& limit = arg == "--proof-max-nodes" ? nodes : witness_values;
    const std::size_t maximum = arg == "--proof-max-nodes" ? 100000 : 16000000;
    if (!parse_size(argv[++index], limit) || limit == 0 || limit > maximum) {
        std::cerr << "invalid MIP proof budget\n";
        return -1;
    }
    return 1;
}
} // namespace markov_cero::apps
