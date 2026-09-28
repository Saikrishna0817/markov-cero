#pragma once

#include "markov_cero/lp/dual/dual_simplex.hpp"
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/milp/milp_solver.hpp"
#include "markov_cero/model/model.hpp"

#include <optional>
#include <limits>
#include <string>
#include <vector>

namespace markov_cero::milp {

struct NodeLpResult {
    lp::reference::SolveStatus status{lp::reference::SolveStatus::infeasible};
    std::vector<double> primal;
    // Row multipliers in original model row order when available.
    std::vector<double> row_dual;
    double objective{0.0};
    // Certified (weak-dual) lower bound used by branch-and-bound. It may be
    // below the primal objective for iterative sparse relaxations.
    double lower_bound{-std::numeric_limits<double>::infinity()};
    std::size_t iterations{0};
    // Condition proxy of the node basis factorization (0.0 = not computed).
    double condition_estimate{0.0};
    // Human-readable reason when the relaxation could not be dispatched or
    // certified. Kept alongside status so callers never have to infer a
    // failure cause from an enum ordinal.
    std::string message;
    std::optional<lp::dual::BasisState> basis;
};

[[nodiscard]] NodeLpResult solve_node_relaxation(
    const model::Model& node_model,
    const Options& options,
    const std::optional<lp::dual::BasisState>& warm_start);

} // namespace markov_cero::milp
