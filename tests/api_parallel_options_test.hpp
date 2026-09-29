#pragma once
#include "../src/api/api_internal.hpp"

inline void test_api_parallel_node_selection_pass_through() {
    markov_cero::core::SolveContext ctx;
    markov_cero::api::SolveOptions options;
    options.engine = "parallel";
    for (const auto policy : {markov_cero::milp::NodeSelection::best_bound,
                              markov_cero::milp::NodeSelection::depth_first,
                              markov_cero::milp::NodeSelection::best_bound_dive}) {
        options.milp_options.node_selection = policy;
        const auto parallel = markov_cero::api::detail::make_parallel_options(options, ctx);
        if (parallel.node_selection != policy)
            throw std::runtime_error("parallel node-selection policy was dropped");
    }
}
