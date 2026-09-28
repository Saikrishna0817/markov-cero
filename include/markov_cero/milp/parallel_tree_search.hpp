#pragma once

#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/milp/shared_incumbent.hpp"
#include "markov_cero/milp/work_queue.hpp"
#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <chrono>
#include <optional>

namespace markov_cero::milp {

struct ParallelOptions {
    std::size_t num_threads{4};
    double time_limit_seconds{60.0};
    std::optional<std::chrono::steady_clock::time_point> deadline;
    std::size_t max_nodes{50000};
    /// Maximum shared-queue nodes; in-flight worker batches are separately bounded.
    std::size_t max_queued_nodes{50000};
    double relative_gap_tolerance{1e-4};
    double absolute_gap_tolerance{1e-6};
    double integrality_tolerance{1e-6};
    double feasibility_tolerance{1e-7};
    std::size_t max_iterations{500000};
    bool enable_warm_start{true};
    bool enable_cuts{true};
    bool enable_mir_cuts{true};
    bool enable_heuristics{true};
    bool enable_strong_branching{true};
    std::size_t max_cut_rounds{5};
    std::size_t max_pump_iterations{10};
    BranchingStrategy branching_strategy{BranchingStrategy::pseudo_cost};
    /// R5 node-selection policy shared by all workers (best-bound by default).
    NodeSelection node_selection{NodeSelection::best_bound};
};

using ParallelResult = Result;

[[nodiscard]] Result solve_parallel(const model::Model& model, const ParallelOptions& options = {});

} // namespace markov_cero::milp
