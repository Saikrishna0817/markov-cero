#pragma once
#include "markov_cero/lp/reference/revised_simplex.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace markov_cero::milp {
// MIP-01 contract §3: prune-time downward guard on a certified node bound.
// Applied only where a bound may kill a subtree, so gap decisions and
// reported bounds keep the certified value itself (an absolute gap tolerance
// of 1e-6 must not be silently consumed by a relative storage guard).
// NaN and -inf pass through unchanged and never satisfy a prune.
inline double prune_guard(double certified_bound) {
    return certified_bound - 1e-10 * (1.0 + std::abs(certified_bound));
}
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
