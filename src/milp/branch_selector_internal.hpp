#pragma once
#include "markov_cero/milp/branch_selector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace markov_cero::milp {
namespace detail_branch_selector {}

std::vector<std::size_t> find_fractional_variables(const std::vector<double>& primal,
                                                   const std::vector<model::VariableType>& types,
                                                   double integrality_tol);
std::size_t select_most_fractional(const std::vector<double>& primal,
                                   const std::vector<std::size_t>& candidates);
std::size_t select_pseudo_cost(const std::vector<double>& primal,
                               const std::vector<std::size_t>& candidates,
                               const std::vector<VariablePseudoCost>& pseudo_costs);
std::size_t select_branching_variable(const std::vector<double>& primal,
                                      const std::vector<model::VariableType>& types,
                                      const std::vector<VariablePseudoCost>& pseudo_costs,
                                      BranchingStrategy strategy, double integrality_tol,
                                      const model::Model* feature_model,
                                      const IBranchingScorer* scorer,
                                      MlBranchingTelemetry* telemetry,
                                      const std::vector<double>* row_duals);
BipartiteGraphFeatures extract_bipartite_features(
    const std::vector<double>& primal,
    const std::vector<model::VariableType>& types,
    const std::vector<std::size_t>& candidates,
    const std::vector<VariablePseudoCost>& pseudo_costs,
    const model::Model& model,
    const std::vector<double>& row_duals);
}
