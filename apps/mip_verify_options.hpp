#pragma once
#include "cli_numbers.hpp"
#include "markov_cero/verify/mip_proof.hpp"
#include <chrono>
#include <string>
#include <vector>

namespace markov_cero::apps {
inline bool parse_mip_verify_options(int argc, char** argv, int& index,
                                    verify::MipProofOptions& options, double& seconds) {
    const std::string arg = argv[index];
    if (arg == "--time-limit") {
        if (index + 1 >= argc || !parse_nonnegative(argv[++index], seconds) ||
            seconds <= 0 || seconds > 1e8) return false;
    } else if (arg == "--max-nodes" || arg == "--max-values") {
        if (index + 1 >= argc) return false;
        std::size_t& limit = arg == "--max-nodes" ? options.maximum_nodes
                                                   : options.maximum_witness_values;
        const std::size_t maximum = arg == "--max-nodes" ? 100000 : 16000000;
        if (!parse_size(argv[++index], limit) || limit == 0 || limit > maximum) return false;
    } else if (arg == "--relative-gap") {
        if (index + 1 >= argc || !parse_nonnegative(argv[++index], options.relative_gap) ||
            options.relative_gap >= 1) return false;
    } else return false;
    return true;
}
} // namespace markov_cero::apps
