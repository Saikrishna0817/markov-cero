#include "cli_options.hpp"

namespace markov_cero::apps {

CliOptions CliOptions::parse(int argc, char** argv) {
    CliOptions parsed;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            usage(std::cout);
            parsed.help_requested = true;
            return parsed;
        }
        if (arg == "--output") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.output_path = argv[++i];
            continue;
        }
        if (arg == "--engine") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.engine_name = argv[++i];
            if (parsed.engine_name != "primal" && parsed.engine_name != "dual" &&
                parsed.engine_name != "ipm" && parsed.engine_name != "pdlp" &&
                parsed.engine_name != "milp" && parsed.engine_name != "parallel" &&
                parsed.engine_name != "qp" && parsed.engine_name != "miqp" &&
                parsed.engine_name != "sqp" && parsed.engine_name != "outer_approx" &&
                parsed.engine_name != "auto") {
                std::cerr << "invalid engine: " << parsed.engine_name << "\n";
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--threads") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (!parse_size(argv[++i], parsed.num_threads)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (parsed.num_threads == 0 || parsed.num_threads > 256) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.threads_explicit = true;
            continue;
        }
        if (arg == "--branching") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            const std::string bval = argv[++i];
            if (bval == "most_fractional") {
                parsed.milp_options.branching_strategy =
                    milp::BranchingStrategy::most_fractional;
            } else if (bval == "pseudo_cost") {
                parsed.milp_options.branching_strategy = milp::BranchingStrategy::pseudo_cost;
            } else if (bval == "strong_branching") {
                parsed.milp_options.branching_strategy =
                    milp::BranchingStrategy::strong_branching;
            } else if (bval == "reliability") {
                parsed.milp_options.branching_strategy = milp::BranchingStrategy::reliability;
            } else if (bval == "ml_gnn") {
                // W2/D-04: ML branching requires explicit opt-in. The
                // scorer itself loads only when compiled with
                // MARKOV_CERO_ENABLE_ML and the model file exists;
                // otherwise the solver falls back to pseudo_cost
                // silently (LOCKED activation contract).
                parsed.milp_options.branching_strategy = milp::BranchingStrategy::ml_gnn;
            } else {
                std::cerr << "invalid branching strategy: " << bval << "\n";
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--node-selection") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            const std::string nval = argv[++i];
            if (nval == "best-bound") {
                parsed.milp_options.node_selection = milp::NodeSelection::best_bound;
            } else if (nval == "depth-first") {
                parsed.milp_options.node_selection = milp::NodeSelection::depth_first;
            } else if (nval == "dive") {
                parsed.milp_options.node_selection =
                    milp::NodeSelection::best_bound_dive;
            } else {
                std::cerr << "invalid node selection: " << nval << "\n";
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--max-nodes") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (!parse_size(argv[++i], parsed.milp_options.max_nodes)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--max-queued-nodes") {
            if (i + 1 >= argc ||
                !parse_size(argv[++i], parsed.milp_options.max_queued_nodes)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--max-input-bytes") {
            std::size_t maximum = 0;
            if (i + 1 >= argc || !parse_size(argv[++i], maximum) ||
                maximum == 0 || maximum > 1073741824ULL) {
                std::cerr << "maximum input bytes must be in 1..1073741824\n";
                parsed.error = true;
                parsed.exit_code = 8;
                return parsed;
            }
            parsed.maximum_input_bytes = maximum;
            continue;
        }
        if (arg == "--memory-limit-bytes") {
            std::size_t maximum = 0;
            if (i + 1 >= argc || !parse_size(argv[++i], maximum) || maximum == 0) {
                std::cerr << "memory limit bytes must be positive\n";
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.memory_limit_bytes = maximum;
            continue;
        }
        if (arg == "--device-memory-limit-bytes") {
            std::size_t maximum = 0;
            if (i + 1 >= argc || !parse_size(argv[++i], maximum) || maximum == 0) {
                std::cerr << "device memory limit bytes must be positive\n";
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.device_memory_limit_bytes = maximum;
            continue;
        }
        const int time_option = parse_solve_time_option(arg, argc, argv, i,
            parsed.time_limit_seconds, parsed.milp_options.time_limit_seconds,
            parsed.options.time_limit_seconds);
        if (time_option != 0) {
            if (time_option < 0) { parsed.error = true; parsed.exit_code = 8; return parsed; }
            continue;
        }
        if (arg == "--mip-gap" || arg == "--relative-gap") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (!parse_nonnegative(argv[++i], parsed.milp_options.relative_gap_tolerance)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        const int proof_option = parse_proof_budget_option(arg, argc, argv, i,
            parsed.mip_proof_time_limit_seconds, parsed.mip_proof_max_nodes,
            parsed.mip_proof_max_witness_values);
        if (proof_option != 0) {
            if (proof_option < 0) { parsed.error = true; parsed.exit_code = 8; return parsed; }
            continue;
        }
        if (arg == "--cuts") {
            parsed.milp_options.enable_cuts = true;
            continue;
        }
        if (arg == "--no-cuts") {
            parsed.milp_options.enable_cuts = false;
            continue;
        }
        if (arg == "--heuristics") {
            parsed.milp_options.enable_heuristics = true;
            continue;
        }
        if (arg == "--no-heuristics") {
            parsed.milp_options.enable_heuristics = false;
            continue;
        }
        if (arg == "--warm-start") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.warm_start_path = argv[++i];
            continue;
        }
        if (arg == "--save-basis") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.save_basis_path = argv[++i];
            continue;
        }
        if (arg == "--presolve") {
            parsed.enable_presolve = true;
            continue;
        }
        if (arg == "--no-presolve") {
            parsed.enable_presolve = false;
            continue;
        }
        if (arg == "--scale") {
            parsed.enable_scale = true;
            continue;
        }
        if (arg == "--no-scale") {
            parsed.enable_scale = false;
            continue;
        }
        if (arg == "--max-presolve-passes") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (!parse_size(argv[++i], parsed.max_presolve_passes)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--ruiz-iterations") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (!parse_size(argv[++i], parsed.ruiz_iterations)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--tolerance") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (!parse_nonnegative(argv[++i], parsed.pdlp_tolerance)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (parsed.pdlp_tolerance <= 0.0) {
                std::cerr << "tolerance must be positive: " << parsed.pdlp_tolerance << "\n";
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--backend") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.backend_name = argv[++i];
            if (parsed.backend_name != "cpu" && parsed.backend_name != "gpu") {
                std::cerr << "invalid backend: " << parsed.backend_name << "\n";
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            continue;
        }
        if (arg == "--iteration-limit") {
            if (i + 1 >= argc) {
                usage(std::cerr);
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            if (!parse_size(argv[++i], parsed.options.iteration_limit)) {
                parsed.error = true; parsed.exit_code = 8; return parsed;
            }
            parsed.milp_options.max_iterations = parsed.options.iteration_limit;
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            usage(std::cerr);
            parsed.error = true; parsed.exit_code = 8; return parsed;
        }
        if (!parsed.path.empty()) {
            usage(std::cerr);
            parsed.error = true; parsed.exit_code = 8; return parsed;
        }
        parsed.path = arg;
    }
    if (parsed.path.empty()) {
        usage(std::cerr);
        parsed.error = true; parsed.exit_code = 8; return parsed;
    }
    return parsed;

}

}
