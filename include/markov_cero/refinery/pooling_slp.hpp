#pragma once

#include "markov_cero/model/model.hpp"

#include <cstddef>
#include <vector>

namespace markov_cero::refinery {

struct HaverlyResult {
    bool converged{false};
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
/// Demonstrates non-linear refinery pooling resolution via sequential outer-approximation.
[[nodiscard]] HaverlyResult solve_haverly_pooling(double initial_quality_guess = 2.0,
                                                  std::size_t max_iterations = 50,
                                                  double tolerance = 1e-5);

} // namespace markov_cero::refinery
