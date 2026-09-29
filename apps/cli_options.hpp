#pragma once
#include "cli_usage.hpp"
#include "cli_numbers.hpp"
#include "cli_proof_options.hpp"

#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/milp/milp_solver.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace markov_cero::apps {

struct CliOptions {
    std::string path;
    std::string output_path;
    std::string engine_name = "auto";
    std::size_t num_threads = 4;
    // True only when the user passed --threads on the command line. The W6
    // auto-dispatch upgrade milp->parallel keys on an explicit request, not on
    // the compiled-in default (see docs/engine_selection.md rule 3).
    bool threads_explicit = false;
    std::string warm_start_path;
    std::string save_basis_path;
    bool enable_presolve = true;
    bool enable_scale = true;
    std::size_t max_presolve_passes = 5;
    std::size_t ruiz_iterations = 10;
    double pdlp_tolerance = 1e-4;
    std::string backend_name = "cpu";
    double time_limit_seconds{60.0};
    double mip_proof_time_limit_seconds{5.0};
    std::size_t mip_proof_max_nodes{10000};
    std::size_t mip_proof_max_witness_values{4000000};
    std::optional<std::size_t> maximum_input_bytes;
    std::optional<std::size_t> memory_limit_bytes;
    lp::reference::Options options;
    milp::Options milp_options;

    bool help_requested = false;
    bool error = false;
    int exit_code = 0;

    static void usage(std::ostream& out) { cli_usage(out); }

    static CliOptions parse(int argc, char** argv);
};

} // namespace markov_cero::apps
