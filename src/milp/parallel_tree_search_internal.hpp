#pragma once
#include "markov_cero/milp/gap.hpp"
#include "markov_cero/milp/node_lp.hpp"
#include "markov_cero/milp/parallel_tree_search.hpp"

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/milp/cuts.hpp"
#include "markov_cero/milp/heuristics.hpp"
#include "markov_cero/milp/strong_branching.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <stop_token>
#include <thread>
#include <vector>

namespace markov_cero::milp {
namespace detail_parallel_tree_search {}
namespace detail_parallel_tree_search {
struct SharedPseudoCosts {
    std::mutex mutex;
    std::vector<VariablePseudoCost> costs;
};
}
namespace detail_parallel_tree_search { NodeLpResult solve_node_lp(const model::Model& model, const ParallelOptions& options,
                           const std::optional<lp::dual::BasisState>& warm_start); }
namespace detail_parallel_tree_search { void process_node(std::shared_ptr<BranchNode>&& node, std::size_t thread_id,
                  const model::Model& root_model, const ParallelOptions& options,
                  ThreadSafeNodeQueue& queue, IncumbentManager& incumbent,
                  std::atomic<std::size_t>& next_node_id,
                  std::atomic<std::size_t>& total_nodes_explored,
                  std::atomic<std::size_t>& total_lp_iterations,
                  std::atomic<std::size_t>& total_heuristics_found,
                  std::atomic<std::size_t>& unresolved_node_lps,
                  std::atomic<double>* worker_bounds,
                  SharedPseudoCosts& shared_pseudo_costs, model::Model& node_model,
                  const std::function<void()>& clear_bound); }
namespace detail_parallel_tree_search { void worker_loop(
    std::size_t thread_id, const model::Model& root_model, const ParallelOptions& options,
    ThreadSafeNodeQueue& queue, IncumbentManager& incumbent, std::atomic<std::size_t>& next_node_id,
    std::atomic<std::size_t>& total_nodes_explored, std::atomic<std::size_t>& total_lp_iterations,
    std::atomic<std::size_t>& total_heuristics_found,
    std::atomic<std::size_t>& unresolved_node_lps,
    std::atomic<bool>& interrupted_search, std::atomic<double>* worker_bounds,
    std::size_t num_threads, SharedPseudoCosts& shared_pseudo_costs,
    const std::chrono::steady_clock::time_point start_time, std::stop_token stop_token); }
Result solve_integer_parallel(const model::Model& model, const ParallelOptions& options,
    std::chrono::steady_clock::time_point start_time);
Result solve_parallel(const model::Model& model, const ParallelOptions& input_options);
}
