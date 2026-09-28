#pragma once
#include "markov_cero/milp/heuristics.hpp"

#include "markov_cero/lp/reference/revised_simplex.hpp"
#include "markov_cero/transform/sparse_canonical_model.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace markov_cero::milp {
namespace detail_heuristics {}

bool check_integer_feasibility(const model::Model& model, const std::vector<double>& primal,
                               double feasibility_tol, double integrality_tol);
double compute_objective(const model::Model& model, const std::vector<double>& primal);
HeuristicResult simple_rounding(const model::Model& model,
                                const std::vector<double>& continuous_primal,
                                double feasibility_tol, double integrality_tol);
HeuristicResult feasibility_pump(const model::Model& model,
                                 const std::vector<double>& continuous_primal,
                                 std::size_t max_iterations, double feasibility_tol,
                                 double integrality_tol);
HeuristicResult local_swap_repair(const model::Model& model,
                                  const std::vector<double>& candidate_primal,
                                  std::size_t max_swaps,
                                  double feasibility_tol,
                                  double integrality_tol);
HeuristicResult coefficient_diving(const model::Model& model,
                                   const std::vector<double>& continuous_primal,
                                   std::size_t max_depth,
                                   double feasibility_tol,
                                   double integrality_tol);
}
