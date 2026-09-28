#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <vector>
#include <string>
#include <limits>

namespace markov_cero::refinery {

struct HaverlyResult {
    bool converged{false};
    bool original_feasible{false};
    double maximum_original_violation{std::numeric_limits<double>::infinity()};
    std::string termination_reason{"iteration_limit"};
    std::size_t iterations{0};
    double final_pool_quality{0.0};
    double profit{0.0};
    double flow_xA{0.0};
    double flow_xB{0.0};
    double flow_y1{0.0};
    double flow_y2{0.0};
    double flow_z1{0.0};
    double flow_z2{0.0};
    double runtime_ms{0.0};
};

/// Solves the Haverly Non-Convex Bilinear Pooling benchmark using Successive Linear Programming (SLP).
/// Fixed-quality LP iteration heuristic; convergence does not prove local or global optimality.
[[nodiscard]] HaverlyResult solve_haverly_pooling(double initial_quality_guess = 2.0,
                                                  std::size_t max_iterations = 50,
                                                  double tolerance = 1e-5);

} // namespace markov_cero::refinery
