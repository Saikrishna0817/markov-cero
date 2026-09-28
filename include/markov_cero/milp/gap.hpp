#pragma once
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace markov_cero::milp {
// Minimization-space bound ordering. Unknown or contradictory bounds must not
// turn into zero gaps. Maximization callers normalize before using this helper.
inline double relative_gap(double incumbent, double lower_bound) {
    if (!std::isfinite(incumbent) || !std::isfinite(lower_bound))
        return std::numeric_limits<double>::infinity();
    const long double delta = static_cast<long double>(incumbent) - lower_bound;
    if (delta < -1e-8L * std::max(1.0, std::abs(incumbent)))
        return std::numeric_limits<double>::infinity();
    return static_cast<double>(std::max(0.0L, delta) / std::max(1.0, std::abs(incumbent)));
}
inline lp::reference::SolveStatus gap_status(double incumbent, double lower_bound,
                                            double absolute_tolerance) {
    const long double gap = static_cast<long double>(incumbent) - lower_bound;
    return std::isfinite(relative_gap(incumbent, lower_bound)) && gap <= absolute_tolerance
        ? lp::reference::SolveStatus::optimal : lp::reference::SolveStatus::gap_satisfied;
}
} // namespace markov_cero::milp
