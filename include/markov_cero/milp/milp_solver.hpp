#pragma once

#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/milp/branch_node.hpp"
#include "markov_cero/milp/branch_selector.hpp"
#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace markov_cero::milp {

struct Options {
    std::size_t max_nodes{50000};
    std::size_t max_iterations{500000};
    double time_limit_seconds{60.0};
    std::optional<std::chrono::steady_clock::time_point> deadline;
    double relative_gap_tolerance{1e-4};
    double absolute_gap_tolerance{1e-6};
    double integrality_tolerance{1e-6};
    double feasibility_tolerance{1e-7};
    bool enable_warm_start{true};
    bool enable_cuts{true};
    bool enable_mir_cuts{true};
    bool enable_heuristics{true};
    bool enable_strong_branching{true};
    std::size_t max_cut_rounds{5};
    std::size_t separation_frequency{1};
    std::size_t max_pool_cuts{40};
    std::size_t max_pump_iterations{10};
    /// In-tree separation safety valve (RW-1): skip separation when the node LP
    /// took more than this many iterations — re-separating on expensive nodes
    /// costs more LP time than the pruning recovers (flugpl-class instances).
    std::size_t separation_max_node_iterations{500};
    /// Skip in-tree separation when the node LP is already near-integral: if the
    /// largest fractionality is below this threshold the node closes by branching
    /// cheaply and cut rows only inflate descendant LPs (RW-1 tuning, flugpl).
    double separation_min_max_fractionality{0.05};
    /// In-tree separation must pay for itself: a node's cut round is only allowed
    /// to spend this multiple of its base LP iteration count on re-solves. If the
    /// cut re-solve exceeds the budget, the rounds stop (keeps total LP work
    /// within a constant factor of the no-cuts run — the flugpl guard).
    double separation_lp_budget_factor{4.0};
    /// Aggregate LP-iteration allowance for in-tree separation across the whole
    /// tree, as a factor of the root LP's iteration count. Bounds the worst-case
    /// total-time regression on instances where in-tree cuts do not pay.
    double separation_total_budget_factor{8.0};
    /// In-tree separation rounds are capped below the root's max_cut_rounds:
    /// deep re-separation has sharply diminishing returns per LP re-solve.
    std::size_t max_in_tree_cut_rounds{1};
    /// Fewer, stronger cuts per in-tree node than at the root: cut rows enlarge
    /// every descendant LP, so per-node budget is a small fraction of max_pool_cuts.
    std::size_t max_in_tree_cuts_per_node{4};
    BranchingStrategy branching_strategy{BranchingStrategy::pseudo_cost};
    /// R5 node-selection policy (see branch_node.hpp). Default best-bound;
    /// depth-first / best-bound-plunge trade a node increase for memory and
    /// early incumbents.
    NodeSelection node_selection{NodeSelection::best_bound};
};

struct Result {
    lp::reference::SolveStatus status{lp::reference::SolveStatus::infeasible};
    std::vector<double> primal;
    double objective{0.0};
    double best_bound{std::numeric_limits<double>::quiet_NaN()};
    double relative_gap{std::numeric_limits<double>::infinity()};
    std::size_t nodes_explored{0};
    std::size_t lp_iterations{0};
    std::size_t cuts_generated{0};
    std::size_t heuristics_found{0};
    double runtime_ms{0.0};
    // Condition proxy (max|Uii|/min|Uii|) of the root LP relaxation basis.
    // 0.0 when no root LP factorization ran.
    double condition_estimate{0.0};
    std::string message;
    bool ml_requested{false};
    bool ml_model_loaded{false};
    MlBranchingTelemetry ml_telemetry;
    std::string ml_fallback_reason;
};

[[nodiscard]] Result solve(const model::Model& model, const Options& options = {});

} // namespace markov_cero::milp
