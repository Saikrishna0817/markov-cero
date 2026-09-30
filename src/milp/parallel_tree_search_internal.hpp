#pragma once
#include "markov_cero/milp/gap.hpp"
#include "markov_cero/milp/node_lp.hpp"
#include "markov_cero/milp/node_propagation.hpp"
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
#include <iterator>
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
// Each worker writes only its own slot; the coordinator merges after joining.
struct ProofEventCollector {
    std::vector<verify::MipObligation> root;
    std::vector<std::vector<verify::MipObligation>> workers;
    explicit ProofEventCollector(std::size_t count) : workers(count) {}

    void record_cuts(std::vector<verify::MipObligation>& destination,
                     std::size_t node, const std::vector<Cut>& cuts,
                     const std::vector<double>& primal) {
        for (const auto& cut : cuts) {
            if (cut.coefficients.size() != primal.size()) continue;
            verify::MipObligation note;
            note.kind = verify::MipObligationKind::cut;
            note.node = node;
            note.coefficients = cut.coefficients;
            note.rhs = cut.rhs;
            for (std::size_t j = 0; j < primal.size(); ++j)
                note.observed_lhs += cut.coefficients[j] * primal[j];
            // §5.4: a recorded obligation must be a finite row violated at the
            // point it was recorded against; anything else is not evidence.
            if (!std::isfinite(note.observed_lhs) || !std::isfinite(note.rhs) ||
                note.observed_lhs >= note.rhs)
                continue;
            destination.push_back(std::move(note));
        }
    }

    void record_propagations(std::size_t worker, const BranchNode& node,
                             const model::Model& root_model,
                             const std::vector<model::Bound>& lower,
                             const std::vector<model::Bound>& upper) {
        auto local_lower = lower;
        auto local_upper = upper;
        auto overlay = node.bounds;
        const auto propagation = propagate_singleton_rows(
            root_model, local_lower, local_upper, overlay);
        for (const auto& step : propagation.steps) {
            verify::MipObligation note;
            note.kind = verify::MipObligationKind::propagation;
            note.node = node.id;
            note.source_row = step.row;
            note.variable = step.variable;
            note.source_coefficient = step.coefficient;
            note.source_rhs = step.source_rhs;
            note.derived_bound = step.derived_bound;
            note.source_is_lower = step.source_is_lower;
            workers[worker].push_back(std::move(note));
        }
    }

    void append_to(Result& result) {
        result.obligations.insert(result.obligations.end(),
                                  std::make_move_iterator(root.begin()),
                                  std::make_move_iterator(root.end()));
        for (auto& notes : workers)
            result.obligations.insert(result.obligations.end(),
                                      std::make_move_iterator(notes.begin()),
                                      std::make_move_iterator(notes.end()));
    }
};
}
namespace detail_parallel_tree_search { NodeLpResult solve_node_lp(const model::Model& model, const ParallelOptions& options,
                           const std::optional<lp::dual::BasisState>& warm_start); }
namespace detail_parallel_tree_search { NodeLpResult solve_node_lp(const model::Model& model,
    const ParallelOptions& options, const std::optional<lp::dual::BasisState>& warm_start,
    const std::vector<model::Bound>& variable_lower,
    const std::vector<model::Bound>& variable_upper); }
namespace detail_parallel_tree_search { void apply_parallel_root_cuts(
    model::Model& root_model, const ParallelOptions& options,
    const NodeLpResult& root_lp, IncumbentManager& incumbent,
    std::atomic<std::size_t>& total_lp_iterations,
    std::vector<double>& current_primal, double& current_obj,
    std::optional<lp::dual::BasisState>& current_basis,
    double& best_lower_bound, std::size_t& root_cuts_generated,
    ProofEventCollector& proof_events); }
namespace detail_parallel_tree_search { NodeLpResult solve_parallel_node_with_cuts(
    BranchNode& node, const model::Model& root_model, const ParallelOptions& options,
    const std::optional<lp::dual::BasisState>& warm_basis,
    const std::vector<model::Bound>& lower, const std::vector<model::Bound>& upper,
    std::size_t explored_count, std::atomic<std::size_t>& total_lp_iterations,
    std::atomic<std::size_t>& total_cuts_generated,
    ProofEventCollector& proof_events, std::size_t thread_id); }
namespace detail_parallel_tree_search { void process_node(std::shared_ptr<BranchNode>&& node, std::size_t thread_id,
                  const model::Model& root_model, const ParallelOptions& options,
                  ThreadSafeNodeQueue& queue, IncumbentManager& incumbent,
                  std::atomic<std::size_t>& next_node_id,
                  std::atomic<std::size_t>& total_nodes_explored,
                  std::atomic<std::size_t>& total_lp_iterations,
                  std::atomic<std::size_t>& total_cuts_generated,
                  std::atomic<std::size_t>& total_heuristics_found,
                  std::atomic<std::size_t>& unresolved_node_lps,
                  std::atomic<double>* worker_bounds,
                  SharedPseudoCosts& shared_pseudo_costs,
                  std::vector<model::Bound>& node_lower,
                  std::vector<model::Bound>& node_upper,
                  NodeBounds::MaterializationScratch& bounds_scratch,
                  const std::function<void()>& clear_bound,
                  ProofEventCollector& proof_events); }
namespace detail_parallel_tree_search { void worker_loop(
    std::size_t thread_id, const model::Model& root_model, const ParallelOptions& options,
    ThreadSafeNodeQueue& queue, IncumbentManager& incumbent, std::atomic<std::size_t>& next_node_id,
    std::atomic<std::size_t>& total_nodes_explored, std::atomic<std::size_t>& total_lp_iterations,
    std::atomic<std::size_t>& total_cuts_generated,
    std::atomic<std::size_t>& total_heuristics_found,
    std::atomic<std::size_t>& unresolved_node_lps,
    std::atomic<bool>& interrupted_search, std::atomic<double>* worker_bounds,
    std::size_t num_threads, SharedPseudoCosts& shared_pseudo_costs,
    const std::chrono::steady_clock::time_point start_time, std::stop_token stop_token,
    ProofEventCollector& proof_events); }
Result solve_integer_parallel(const model::Model& model, const ParallelOptions& options,
    std::chrono::steady_clock::time_point start_time);
Result solve_parallel(const model::Model& model, const ParallelOptions& input_options);

// Result assembly: classify the final tree state into status, incumbent, global
// bound and gap. Defined in parallel_tree_search_result.cpp.
void classify_parallel_result(
    Result& result, const ParallelOptions& options, ThreadSafeNodeQueue& queue,
    IncumbentManager& incumbent, const std::atomic<double>* worker_bounds,
    std::size_t num_threads, double best_lower_bound,
    const std::atomic<std::size_t>& unresolved_node_lps,
    const std::atomic<bool>& interrupted_search);
}
