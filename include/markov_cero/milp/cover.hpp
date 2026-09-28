#pragma once

#include "markov_cero/milp/cut_pool.hpp"
#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <vector>

namespace markov_cero::milp {

/// Generates minimal knapsack cover inequalities for binary variables.
/// Cornuéjols (2008), Wolsey (1998): If sum_{j in C} a_j > b for binary x_j in a row
/// sum a_j x_j <= b, then sum_{j in C} x_j <= |C| - 1 is a valid cut.
[[nodiscard]] std::vector<Cut>
generate_cover_cuts(const model::Model& model,
                    const std::vector<double>& original_primal,
                    std::size_t max_cuts = 10,
                    double min_violation = 1e-4);

} // namespace markov_cero::milp
